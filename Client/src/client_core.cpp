#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "client_core.h"
#include "lora_mesh.h"
#include "packet.h"
#include "crypto.h"
#include "ui_terminal.h"

static const char* TAG = TAG_MAIN;
static ClientState_t g_state;
static bool g_initialized = false;

Status_t client_init(void)
{
    if (g_initialized) return STATUS_OK;
    
    memset(&g_state, 0, sizeof(g_state));
    g_state.connected_to_server = false;
    g_state.node_count = 0;
    strcpy(g_state.username, NODE_NAME);
    g_state.logged_in = false;
    g_state.current_screen = BBS_SCREEN_LOGIN;
    
    g_state.known_nodes[0].id = MY_NODE_ID;
    strncpy(g_state.known_nodes[0].name, NODE_NAME, MAX_USERNAME_LEN - 1);
    g_state.known_nodes[0].last_seen = esp_timer_get_time() / 1000;
    g_state.known_nodes[0].is_server = false;
    g_state.node_count = 1;
    
    ESP_LOGI(TAG, "Client initialized. ID: %d", MY_NODE_ID);
    g_initialized = true;
    
    return STATUS_OK;
}

Status_t client_loop(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - g_state.last_beacon > BEACON_INTERVAL) {
        client_send_beacon();
        g_state.last_beacon = now;
    }
    
    return STATUS_OK;
}

Status_t client_navigate(BbsScreen_t screen)
{
    g_state.current_screen = screen;
    
    switch (screen) {
        case BBS_SCREEN_LOGIN:
            ui_draw_login_screen();
            break;
        case BBS_SCREEN_MAIN:
            ui_draw_main_menu();
            break;
        case BBS_SCREEN_MAIL:
            ui_draw_mail_screen();
            break;
        case BBS_SCREEN_BOARD:
            ui_draw_board_screen();
            break;
        case BBS_SCREEN_FILES:
            ui_draw_files_screen();
            break;
        case BBS_SCREEN_NODES:
            ui_draw_nodes_screen();
            // Обновить список узлов
            for (int i = 0; i < g_state.node_count && i < 5; i++) {
                char line[80];
                const char* status = g_state.known_nodes[i].is_server ? ANSI_GREEN : ANSI_WHITE;
                snprintf(line, sizeof(line), "║ %-4d %-23s %-11s %lu sec ago     ║",
                         g_state.known_nodes[i].id,
                         g_state.known_nodes[i].name,
                         g_state.known_nodes[i].is_server ? "ONLINE" : "ONLINE",
                         (esp_timer_get_time() / 1000 - g_state.known_nodes[i].last_seen) / 1000);
                ui_print_color_at(8, 7 + i, line, status);
            }
            break;
        default:
            break;
    }
    
    return STATUS_OK;
}

Status_t client_send_beacon(void)
{
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_BEACON, MY_NODE_ID, BROADCAST_ID, strlen(NODE_NAME));
    memcpy(pkt.payload, NODE_NAME, strlen(NODE_NAME));
    pkt.header.payload_len = strlen(NODE_NAME);
    
    const CryptoIdentity_t* identity = crypto_get_identity();
    if (identity && identity->initialized) {
        packet_set_flag(&pkt.header, PKT_FLAG_SIGNED);
        crypto_sign_message(identity->sign_private, 
                           pkt.payload, 
                           pkt.header.payload_len, 
                           pkt.signature);
    }
    
    return lora_mesh_send(&pkt);
}

Status_t client_send_chat(const char* msg)
{
    if (!msg || strlen(msg) == 0) return STATUS_INVALID;
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_CHAT, MY_NODE_ID, BROADCAST_ID, strlen(msg));
    memcpy(pkt.payload, msg, strlen(msg));
    pkt.header.payload_len = strlen(msg);
    
    return lora_mesh_send(&pkt);
}

Status_t client_request_mail(uint16_t target_id)
{
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_MAIL_REQ, MY_NODE_ID, target_id, 0);
    return lora_mesh_send(&pkt);
}

Status_t client_ping_server(void)
{
    return lora_mesh_send_ping(SERVER_NODE_ID);
}

Status_t client_handle_packet(const uint8_t* buffer, size_t len)
{
    FullPacket_t pkt;
    if (packet_deserialize(buffer, len, &pkt) != STATUS_OK) {
        return STATUS_ERROR;
    }
    
    if (pkt.header.dst != MY_NODE_ID && pkt.header.dst != BROADCAST_ID) {
        return STATUS_OK;
    }
    
    switch (pkt.header.type) {
        case PKT_BEACON:
        case PKT_NODE:
        case 9: {
            uint16_t sender_id = pkt.header.src;
            char sender_name[MAX_USERNAME_LEN];
            
            if (pkt.header.payload_len < MAX_USERNAME_LEN) {
                uint8_t name_len = (pkt.header.payload_len < MAX_USERNAME_LEN - 1) ? 
                                   pkt.header.payload_len : MAX_USERNAME_LEN - 1;
                memcpy(sender_name, pkt.payload, name_len);
                sender_name[name_len] = '\0';
            } else {
                strncpy(sender_name, (char*)pkt.payload, MAX_USERNAME_LEN - 1);
                sender_name[MAX_USERNAME_LEN - 1] = '\0';
            }
            
            int found_index = -1;
            for (int i = 0; i < g_state.node_count; i++) {
                if (g_state.known_nodes[i].id == sender_id) {
                    found_index = i;
                    break;
                }
            }
            
            if (found_index >= 0) {
                strcpy(g_state.known_nodes[found_index].name, sender_name);
                g_state.known_nodes[found_index].last_seen = esp_timer_get_time() / 1000;
            } else {
                if (g_state.node_count < MAX_KNOWN_NODES) {
                    bool name_conflict = false;
                    int conflict_with_id = 0;
                    
                    for (int i = 0; i < g_state.node_count; i++) {
                        if (strcmp(g_state.known_nodes[i].name, sender_name) == 0) {
                            name_conflict = true;
                            conflict_with_id = g_state.known_nodes[i].id;
                            break;
                        }
                    }
                    
                    g_state.known_nodes[g_state.node_count].id = sender_id;
                    strcpy(g_state.known_nodes[g_state.node_count].name, sender_name);
                    g_state.known_nodes[g_state.node_count].last_seen = esp_timer_get_time() / 1000;
                    g_state.known_nodes[g_state.node_count].is_server = (sender_id == SERVER_NODE_ID);
                    g_state.node_count++;
                    
                    if (name_conflict) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Name conflict! Node %d also uses '%s'", 
                                 conflict_with_id, sender_name);
                        ui_print_color_at(10, 23, msg, ANSI_RED);
                    } else {
                        if (sender_id == SERVER_NODE_ID) {
                            g_state.connected_to_server = true;
                            ui_print_color_at(10, 23, "✅ SERVER DETECTED!", ANSI_GREEN ANSI_BOLD);
                        }
                    }
                    
                    // Обновить экран узлов если открыт
                    if (g_state.current_screen == BBS_SCREEN_NODES) {
                        client_navigate(BBS_SCREEN_NODES);
                    }
                }
            }
            break;
        }
        
        case PKT_CHAT: {
            char sender_name[MAX_USERNAME_LEN] = "Unknown";
            KnownNode_t* node = client_find_node(pkt.header.src);
            if (node) {
                strncpy(sender_name, node->name, MAX_USERNAME_LEN - 1);
            }
            
            ui_print_color_at(10, 23, "[CHAT] ", ANSI_YELLOW);
            ui_print_color(sender_name, ANSI_CYAN);
            ui_print(": ");
            pkt.payload[pkt.header.payload_len] = '\0';
            ui_print((char*)pkt.payload);
            ui_beep();
            break;
        }
        
        case PKT_PONG: {
            if (pkt.header.src == SERVER_NODE_ID) {
                g_state.connected_to_server = true;
                ui_print_color_at(10, 23, "✅ Server responded!", ANSI_GREEN);
            }
            break;
        }
        
        case PKT_MAIL_DATA: {
            ui_print_color_at(10, 23, "📧 Mail received!", ANSI_GREEN);
            ui_beep();
            break;
        }
        
        default:
            break;
    }
    
    return STATUS_OK;
}

ClientState_t* client_get_state(void)
{
    return &g_state;
}

bool client_is_connected(void)
{
    return g_state.connected_to_server;
}

KnownNode_t* client_find_node(uint16_t node_id)
{
    for (int i = 0; i < g_state.node_count; i++) {
        if (g_state.known_nodes[i].id == node_id) {
            return &g_state.known_nodes[i];
        }
    }
    return NULL;
}