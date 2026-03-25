#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "routing.h"
#include "lora_mesh.h"
#include "packet.h"
#include "crypto.h"

static const char* TAG = TAG_ROUTING;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static RoutingTable_t g_routing_table;
static NodeInfo_t g_nodes[MAX_NODE_ENTRIES];
static uint8_t g_node_count = 0;
static bool g_initialized = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ МАРШРУТИЗАЦИИ
// ============================================================================
Status_t routing_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing routing subsystem...");
    
    memset(&g_routing_table, 0, sizeof(g_routing_table));
    memset(g_nodes, 0, sizeof(g_nodes));
    g_node_count = 0;
    
    g_routing_table.last_update = esp_timer_get_time() / 1000;
    g_initialized = true;
    
    ESP_LOGI(TAG, "Routing subsystem initialized");
    
    return STATUS_OK;
}

// ============================================================================
// ОБРАБОТКА ВХОДЯЩЕГО ПАКЕТА
// ============================================================================
Status_t routing_process_packet(const PacketHeader_t* hdr, uint16_t next_hop)
{
    if (!hdr) {
        return STATUS_INVALID;
    }
    
    routing_update_channel_quality(next_hop, 255 - hdr->hops);
    routing_add_route(hdr->src, next_hop, hdr->hops, 255 - hdr->hops);
    
    return STATUS_OK;
}

// ============================================================================
// ПОИСК МАРШРУТА
// ============================================================================
Status_t routing_find_route(uint16_t dest, uint16_t* via, uint8_t* hops)
{
    if (!via || !hops) {
        return STATUS_INVALID;
    }
    
    if (dest == BROADCAST_ID) {
        *via = BROADCAST_ID;
        *hops = 0;
        return STATUS_OK;
    }
    
    for (uint8_t i = 0; i < g_routing_table.count; i++) {
        if (g_routing_table.entries[i].dest == dest) {
            uint32_t now = esp_timer_get_time() / 1000;
            if (now - g_routing_table.entries[i].last_seen < ROUTE_TIMEOUT) {
                *via = g_routing_table.entries[i].via;
                *hops = g_routing_table.entries[i].hops;
                return STATUS_OK;
            }
        }
    }
    
    return STATUS_NO_ROUTE;
}

// ============================================================================
// ДОБАВЛЕНИЕ МАРШРУТА
// ============================================================================
Status_t routing_add_route(uint16_t dest, uint16_t via, uint8_t hops, uint8_t quality)
{
    if (dest == MY_NODE_ID) {
        return STATUS_INVALID;
    }
    
    uint32_t now = esp_timer_get_time() / 1000;
    
    for (uint8_t i = 0; i < g_routing_table.count; i++) {
        if (g_routing_table.entries[i].dest == dest) {
            if (hops < g_routing_table.entries[i].hops || quality > g_routing_table.entries[i].quality) {
                g_routing_table.entries[i].via = via;
                g_routing_table.entries[i].hops = hops;
                g_routing_table.entries[i].quality = quality;
                g_routing_table.entries[i].last_seen = now;
                DBG_LOG(TAG, "Route updated: %d via %d hops %d", dest, via, hops);
            }
            return STATUS_OK;
        }
    }
    
    if (g_routing_table.count < MAX_ROUTE_ENTRIES) {
        g_routing_table.entries[g_routing_table.count].dest = dest;
        g_routing_table.entries[g_routing_table.count].via = via;
        g_routing_table.entries[g_routing_table.count].hops = hops;
        g_routing_table.entries[g_routing_table.count].quality = quality;
        g_routing_table.entries[g_routing_table.count].last_seen = now;
        g_routing_table.count++;
        g_routing_table.last_update = now;
        
        DBG_LOG(TAG, "Route added: %d via %d hops %d", dest, via, hops);
        return STATUS_OK;
    }
    
    return STATUS_QUEUE_FULL;
}

// ============================================================================
// ОЧИСТКА МАРШРУТОВ
// ============================================================================
void routing_cleanup(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    uint8_t new_count = 0;
    
    for (uint8_t i = 0; i < g_routing_table.count; i++) {
        if (now - g_routing_table.entries[i].last_seen < ROUTE_TIMEOUT) {
            g_routing_table.entries[new_count] = g_routing_table.entries[i];
            new_count++;
        }
    }
    
    g_routing_table.count = new_count;
    DBG_LOG(TAG, "Routing table cleaned: %d entries", new_count);
}

// ============================================================================
// ПОЛУЧЕНИЕ ТАБЛИЦЫ МАРШРУТОВ
// ============================================================================
const RoutingTable_t* routing_get_table(void)
{
    return &g_routing_table;
}

// ============================================================================
// РАССЫЛКА ТАБЛИЦЫ МАРШРУТОВ
// ============================================================================
Status_t routing_broadcast_table(void)
{
    if (g_routing_table.count == 0) {
        return STATUS_OK;
    }
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_ROUTE, MY_NODE_ID, BROADCAST_ID,
                       g_routing_table.count * sizeof(RouteEntry_t));
    pkt.header.flags = PKT_FLAG_BROADCAST;
    pkt.header.ttl = ROUTE_TTL;
    
    memcpy(pkt.payload, g_routing_table.entries,
           g_routing_table.count * sizeof(RouteEntry_t));
    
    Status_t status = lora_mesh_send(&pkt);
    DBG_LOG(TAG, "Route table broadcast: %s", status == STATUS_OK ? "OK" : "FAIL");
    
    return status;
}

// ============================================================================
// ЗАПРОС МАРШРУТА
// ============================================================================
Status_t routing_request_route(uint16_t dest)
{
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_ROUTE_REQ, MY_NODE_ID, BROADCAST_ID, sizeof(uint16_t));
    pkt.header.flags = PKT_FLAG_BROADCAST;
    
    uint16_t* payload = (uint16_t*)pkt.payload;
    *payload = dest;
    pkt.header.payload_len = sizeof(uint16_t);
    
    return lora_mesh_send(&pkt);
}

// ============================================================================
// ОБРАБОТКА ЗАПРОСА МАРШРУТА
// ============================================================================
Status_t routing_handle_route_request(uint16_t from, uint16_t dest)
{
    uint16_t via;
    uint8_t hops;
    
    if (routing_find_route(dest, &via, &hops) == STATUS_OK) {
        FullPacket_t pkt;
        RouteEntry_t entry;
        entry.dest = dest;
        entry.via = via;
        entry.hops = hops;
        entry.quality = 255;
        entry.last_seen = esp_timer_get_time() / 1000;
        
        packet_init_header(&pkt.header, PKT_ROUTE, MY_NODE_ID, from, sizeof(RouteEntry_t));
        memcpy(pkt.payload, &entry, sizeof(RouteEntry_t));
        pkt.header.payload_len = sizeof(RouteEntry_t);
        
        return lora_mesh_send(&pkt);
    }
    
    return STATUS_NO_ROUTE;
}

// ============================================================================
// АЛГОРИТМ ДЕЙКСТРЫ
// ============================================================================
Status_t routing_compute_dijkstra(uint16_t start_node)
{
    DBG_LOG(TAG, "Dijkstra computation for node %d", start_node);
    return STATUS_OK;
}

// ============================================================================
// ОБНОВЛЕНИЕ КАЧЕСТВА КАНАЛА
// ============================================================================
Status_t routing_update_channel_quality(uint16_t node, uint8_t quality)
{
    for (uint8_t i = 0; i < g_routing_table.count; i++) {
        if (g_routing_table.entries[i].via == node) {
            g_routing_table.entries[i].quality = 
                (g_routing_table.entries[i].quality * 3 + quality) / 4;
            return STATUS_OK;
        }
    }
    
    return STATUS_NOT_FOUND;
}

// ============================================================================
// ДОБАВЛЕНИЕ УЗЛА
// ============================================================================
Status_t routing_add_node(const NodeInfo_t* node)
{
    if (!node) {
        return STATUS_INVALID;
    }
    
    for (uint8_t i = 0; i < g_node_count; i++) {
        if (g_nodes[i].node_id == node->node_id) {
            g_nodes[i] = *node;
            DBG_LOG(TAG, "Node updated: %d", node->node_id);
            return STATUS_OK;
        }
    }
    
    if (g_node_count < MAX_NODE_ENTRIES) {
        g_nodes[g_node_count] = *node;
        g_node_count++;
        DBG_LOG(TAG, "Node added: %d (%s)", node->node_id, node->name);
        return STATUS_OK;
    }
    
    return STATUS_QUEUE_FULL;
}

// ============================================================================
// ПОИСК УЗЛА
// ============================================================================
const NodeInfo_t* routing_find_node(uint16_t node_id)
{
    for (uint8_t i = 0; i < g_node_count; i++) {
        if (g_nodes[i].node_id == node_id) {
            return &g_nodes[i];
        }
    }
    
    return NULL;
}

// ============================================================================
// РАССЫЛКА ИНФОРМАЦИИ ОБ УЗЛЕ
// ============================================================================
Status_t routing_broadcast_node_info(void)
{
    FullPacket_t pkt;
    NodeInfo_t node_info;
    
    memset(&node_info, 0, sizeof(node_info));
    node_info.node_id = MY_NODE_ID;
    strncpy(node_info.name, NODE_NAME, sizeof(node_info.name) - 1);
    strncpy((char*)node_info.version, NODE_VERSION, sizeof(node_info.version) - 1);
    node_info.uptime = esp_timer_get_time() / 1000;
    
    const CryptoIdentity_t* identity = crypto_get_identity();
    if (identity) {
        memcpy(node_info.public_key, identity->public_key, KEY_SIZE);
        memcpy(node_info.sign_public, identity->sign_public, KEY_SIZE);
    }
    
    packet_init_header(&pkt.header, PKT_NODE, MY_NODE_ID, BROADCAST_ID, sizeof(NodeInfo_t));
    pkt.header.flags = PKT_FLAG_SIGNED | PKT_FLAG_BROADCAST;
    memcpy(pkt.payload, &node_info, sizeof(NodeInfo_t));
    pkt.header.payload_len = sizeof(NodeInfo_t);
    
    Status_t status = lora_mesh_send(&pkt);
    DBG_LOG(TAG, "Node info broadcast: %s", status == STATUS_OK ? "OK" : "FAIL");
    
    return status;
}

// ============================================================================
// ПОЛУЧЕНИЕ КОЛИЧЕСТВА УЗЛОВ
// ============================================================================
uint8_t routing_get_node_count(void)
{
    return g_node_count;
}