#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

#include "config.h"
#include "sysop.h"
#include "routing.h"
#include "store_forward.h"
#include "netmail.h"
#include "file_transfer.h"
#include "lora_mesh.h"
#include "packet.h"

static const char* TAG = TAG_SYSOP;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static uint16_t g_sysop_users[10];
static uint8_t g_sysop_count = 0;
static bool g_initialized = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ SYSOP
// ============================================================================
Status_t sysop_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing SysOp subsystem...");
    
    g_sysop_users[0] = MY_NODE_ID;
    g_sysop_count = 1;
    
    g_initialized = true;
    ESP_LOGI(TAG, "SysOp initialized");
    
    return STATUS_OK;
}

// ============================================================================
// ОБРАБОТКА УДАЛЕННОЙ КОМАНДЫ
// ============================================================================
Status_t sysop_handle_remote_cmd(uint16_t from, const uint8_t* cmd, size_t len)
{
    if (!cmd || !sysop_check_privileges(from)) {
        DBG_WARN(TAG, "Unauthorized command from %d", from);
        return STATUS_INVALID;
    }
    
    char command[64];
    strncpy(command, (char*)cmd, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';
    
    DBG_LOG(TAG, "Remote command from %d: %s", from, command);
    
    if (strcmp(command, "STATUS") == 0) {
        sysop_send_status(from);
    } else if (strcmp(command, "ROUTES") == 0) {
        sysop_send_routes(from);
    } else if (strcmp(command, "NODES") == 0) {
        sysop_send_nodes(from);
    } else if (strcmp(command, "REBOOT") == 0) {
        ESP_LOGI(TAG, "Remote reboot requested");
        esp_restart();
    } else if (strcmp(command, "LOGS") == 0) {
        sysop_send_logs(from, 50);
    }
    
    return STATUS_OK;
}

// ============================================================================
// ОТПРАВКА СТАТУСА
// ============================================================================
Status_t sysop_send_status(uint16_t to)
{
    char status[512];
    uint32_t uptime = (esp_timer_get_time() / 1000) / 1000;
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    
    snprintf(status, sizeof(status),
             "=== Node Status ===\r\n"
             "Node: %s\r\n"
             "ID: %d\r\n"
             "Uptime: %lu seconds\r\n"
             "Free Heap: %zu bytes\r\n"
             "Stored Packets: %d\r\n"
             "Mail Inbox: %d\r\n"
             "Mail Outbox: %d\r\n"
             "Nodes: %d\r\n",
             NODE_NAME,
             MY_NODE_ID,
             (unsigned long)uptime,
             free_heap,
             store_forward_get_count(),
             netmail_get_inbox_count(),
             netmail_get_outbox_count(),
             routing_get_node_count());
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_SYSOP, MY_NODE_ID, to, strlen(status));
    pkt.header.flags = PKT_FLAG_SIGNED;
    memcpy(pkt.payload, status, strlen(status));
    pkt.header.payload_len = strlen(status);
    
    return lora_mesh_send(&pkt);
}

// ============================================================================
// ОТПРАВКА МАРШРУТОВ
// ============================================================================
Status_t sysop_send_routes(uint16_t to)
{
    const RoutingTable_t* table = routing_get_table();
    
    char routes[1024];
    snprintf(routes, sizeof(routes), "=== Routing Table ===\r\nEntries: %d\r\n\r\n", table->count);
    
    for (uint8_t i = 0; i < table->count; i++) {
        char line[128];
        snprintf(line, sizeof(line), "Dest: %d Via: %d Hops: %d Quality: %d\r\n",
                 table->entries[i].dest,
                 table->entries[i].via,
                 table->entries[i].hops,
                 table->entries[i].quality);
        strncat(routes, line, sizeof(routes) - strlen(routes) - 1);
    }
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_SYSOP, MY_NODE_ID, to, strlen(routes));
    pkt.header.flags = PKT_FLAG_SIGNED;
    memcpy(pkt.payload, routes, strlen(routes));
    pkt.header.payload_len = strlen(routes);
    
    return lora_mesh_send(&pkt);
}

// ============================================================================
// ОТПРАВКА СПИСКА УЗЛОВ
// ============================================================================
Status_t sysop_send_nodes(uint16_t to)
{
    uint8_t count = routing_get_node_count();
    
    char nodes[512];
    snprintf(nodes, sizeof(nodes), "=== Known Nodes ===\r\nCount: %d\r\n", count);
    
    for (uint8_t i = 0; i < count; i++) {
        const NodeInfo_t* node = routing_find_node(i);
        if (node) {
            char line[128];
            snprintf(line, sizeof(line), "ID: %d Name: %s\r\n", node->node_id, node->name);
            strncat(nodes, line, sizeof(nodes) - strlen(nodes) - 1);
        }
    }
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_SYSOP, MY_NODE_ID, to, strlen(nodes));
    pkt.header.flags = PKT_FLAG_SIGNED;
    memcpy(pkt.payload, nodes, strlen(nodes));
    pkt.header.payload_len = strlen(nodes);
    
    return lora_mesh_send(&pkt);
}

// ============================================================================
// ОТПРАВКА ЛОГОВ
// ============================================================================
Status_t sysop_send_logs(uint16_t to, uint16_t lines)
{
    char logs[1024];
    snprintf(logs, sizeof(logs), "=== System Logs ===\r\nLast %d lines\r\n", lines);
    
    strncat(logs, "(Log file reading not implemented)\r\n", sizeof(logs) - strlen(logs) - 1);
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_SYSOP, MY_NODE_ID, to, strlen(logs));
    pkt.header.flags = PKT_FLAG_SIGNED;
    memcpy(pkt.payload, logs, strlen(logs));
    pkt.header.payload_len = strlen(logs);
    
    return lora_mesh_send(&pkt);
}

// ============================================================================
// ВЫПОЛНЕНИЕ ЛОКАЛЬНОЙ КОМАНДЫ
// ============================================================================
Status_t sysop_execute_command(SysOpCommand_t cmd, const char* args)
{
    switch (cmd) {
        case SYSOP_CMD_STATUS:
            ESP_LOGI(TAG, "System Status:");
            ESP_LOGI(TAG, "Free Heap: %zu", heap_caps_get_free_size(MALLOC_CAP_8BIT));
            ESP_LOGI(TAG, "Stored Packets: %d", store_forward_get_count());
            break;
            
        case SYSOP_CMD_ROUTES:
            ESP_LOGI(TAG, "Routing Table: %d entries", routing_get_table()->count);
            break;
            
        case SYSOP_CMD_NODES:
            ESP_LOGI(TAG, "Known Nodes: %d", routing_get_node_count());
            break;
            
        case SYSOP_CMD_REBOOT:
            ESP_LOGI(TAG, "Rebooting...");
            esp_restart();
            break;
            
        case SYSOP_CMD_RESET:
            ESP_LOGI(TAG, "Factory reset requested");
            break;
            
        default:
            return STATUS_INVALID;
    }
    
    return STATUS_OK;
}

// ============================================================================
// ПРОВЕРКА ПРАВ SYSOP
// ============================================================================
bool sysop_check_privileges(uint16_t user_id)
{
    if (user_id == MY_NODE_ID) {
        return true;
    }
    
    for (uint8_t i = 0; i < g_sysop_count; i++) {
        if (g_sysop_users[i] == user_id) {
            return true;
        }
    }
    
    return false;
}