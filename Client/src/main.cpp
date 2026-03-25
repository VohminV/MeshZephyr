#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "config.h"
#include "packet.h"
#include "crypto.h"
#include "lora_mesh.h"
#include "client_core.h"
#include "ui_terminal.h"

static const char* TAG = TAG_MAIN;

static void lora_rx_task(void* pvParameters)
{
    while (1) {
        FullPacket_t pkt;
        if (lora_mesh_receive(&pkt) == STATUS_OK) {
            uint8_t raw_buf[MAX_PACKET_SIZE];
            int len = packet_serialize(&pkt, raw_buf, MAX_PACKET_SIZE);
            if (len > 0) {
                client_handle_packet(raw_buf, len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

extern "C" void app_main(void)
{
    nvs_flash_init();
    
    ESP_LOGI(TAG, "Initializing crypto...");
    crypto_init();
    
    ESP_LOGI(TAG, "Initializing LoRa...");
    lora_mesh_init();
    
    ESP_LOGI(TAG, "Initializing UI...");
    ui_init();
    
    ESP_LOGI(TAG, "Initializing client...");
    client_init();
    
    // Экран загрузки
    ui_clear();
    ui_draw_box(20, 8, 40, 8, " SYSTEM ");
    ui_print_color_at(25, 10, "MeshZephyr BBS", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(25, 11, "Loading...", ANSI_CYAN);
    ui_print_color_at(25, 12, "Node: ", ANSI_GREEN);
    
    char node_str[10];
    snprintf(node_str, sizeof(node_str), "%d", MY_NODE_ID);
    ui_print(node_str);
    
    ui_delay_ms(2000);
    
    xTaskCreate(lora_rx_task, "lora_rx_task", 8192, NULL, 5, NULL);
    
    client_send_beacon();
    
    // Главный цикл
    ClientState_t* st = client_get_state();
    
    while (1) {
        client_loop();
        
        // Навигация по экранам
        if (!st->logged_in) {
            ui_draw_login_screen();
            ui_print_color_at(25, 11, "Enter Name: ", ANSI_WHITE);
            ui_get_line(st->username, MAX_USERNAME_LEN);
            
            if (strlen(st->username) > 0) {
                st->logged_in = true;
                client_navigate(BBS_SCREEN_MAIN);
            }
        } else {
            char cmd = ui_get_hotkey();
            cmd = toupper(cmd);
            
            switch (cmd) {
                case 'A': case 'B': case 'C': case 'D': // Scan
                    ui_flash_line(24, 3);
                    ui_print_color_at(10, 23, "Scanning...", ANSI_YELLOW);
                    ui_delay_ms(1500);
                    client_navigate(BBS_SCREEN_MAIN);
                    break;
                    
                case 'R': // Read Mail
                    if (st->current_screen == BBS_SCREEN_MAIN) {
                        client_navigate(BBS_SCREEN_MAIL);
                    }
                    break;
                    
                case 'S': // Send Mail
                    if (st->current_screen == BBS_SCREEN_MAIN) {
                        client_navigate(BBS_SCREEN_MAIL);
                    }
                    break;
                    
                case 'E': // Conference
                case 'X':
                case 'J':
                    ui_print_color_at(10, 23, "Conference feature...", ANSI_YELLOW);
                    ui_delay_ms(1500);
                    client_navigate(BBS_SCREEN_MAIN);
                    break;
                    
                case 'F': // Files
                case 'T':
                case 'V':
                    client_navigate(BBS_SCREEN_FILES);
                    break;
                    
                case 'H': // Chat
                case 'O': {
                    ui_print_color_at(10, 23, "Message: ", ANSI_WHITE);
                    char msg[100];
                    ui_get_line(msg, 100);
                    if (strlen(msg) > 0) {
                        client_send_chat(msg);
                    }
                    client_navigate(BBS_SCREEN_MAIN);
                    break;
                }
                    
                case 'U': // User Info
                case 'W': // Who's On
                    client_navigate(BBS_SCREEN_NODES);
                    break;
                    
                case 'I': // System Info
                    ui_print_color_at(10, 23, "System Info: ESP32 LoRa BBS", ANSI_CYAN);
                    ui_delay_ms(2000);
                    client_navigate(BBS_SCREEN_MAIN);
                    break;
                    
                case 'Q': // Quit
                    ui_clear();
                    ui_draw_box(20, 10, 40, 5, " GOODBYE ");
                    ui_print_color_at(25, 12, "Thank you for calling!", ANSI_YELLOW);
                    ui_delay_ms(2000);
                    esp_restart();
                    break;
                    
                case 27: // ESC
                    client_navigate(BBS_SCREEN_MAIN);
                    break;
                    
                default:
                    ui_beep();
                    break;
            }
        }
    }
}