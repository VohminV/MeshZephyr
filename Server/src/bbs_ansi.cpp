#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "bbs_ansi.h"
#include "netmail.h"
#include "message_board.h"
#include "file_transfer.h"
#include "sysop.h"
#include "routing.h"

static const char* TAG = TAG_BBS;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static BbsSession_t g_session;
static char g_input_buffer[256];
static uint8_t g_input_pos = 0;
static bool g_initialized = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ BBS
// ============================================================================
Status_t bbs_ansi_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing BBS ANSI subsystem...");
    
    // Правильная инициализация uart_config_t для ESP-IDF
    uart_config_t uart_config;
    memset(&uart_config, 0, sizeof(uart_config));
    uart_config.baud_rate = SERIAL_BAUD;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    uart_config.rx_flow_ctrl_thresh = 0;
    // Не устанавливаем flags - оно уже 0 после memset
    
    uart_driver_install(UART_NUM_0, 256, 256, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    
    // Инициализация сессии
    memset(&g_session, 0, sizeof(g_session));
    g_session.state = BBS_STATE_LOGIN;
    g_input_pos = 0;
    
    g_initialized = true;
    
    // Показ экрана входа
    bbs_ansi_login_screen();
    
    ESP_LOGI(TAG, "BBS ANSI initialized");
    
    return STATUS_OK;
}

// ============================================================================
// ОБРАБОТКА ВВОДА
// ============================================================================
Status_t bbs_ansi_process(void)
{
    if (!g_initialized) {
        return STATUS_ERROR;
    }
    
    uint8_t byte;
    while (uart_read_bytes(UART_NUM_0, &byte, 1, 0) > 0) {
        char c = (char)byte;
        
        if (c == '\r' || c == '\n') {
            g_input_buffer[g_input_pos] = '\0';
            bbs_ansi_print("\r\n");
            
            if (g_session.state == BBS_STATE_LOGIN) {
                if (g_input_pos > 0) {
                    strncpy(g_session.username, g_input_buffer, sizeof(g_session.username) - 1);
                    g_session.logged_in = true;
                    g_session.login_time = esp_timer_get_time() / 1000;
                    g_session.last_activity = g_session.login_time;
                    bbs_ansi_show_menu();
                }
            } else if (g_session.state == BBS_STATE_MENU) {
                bbs_ansi_handle_command(g_input_buffer[0]);
            }
            
            g_input_pos = 0;
        } else if (c == '\b' || c == 127) {
            if (g_input_pos > 0) {
                g_input_pos--;
                bbs_ansi_print("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            if (g_input_pos < sizeof(g_input_buffer) - 1) {
                g_input_buffer[g_input_pos] = c;
                g_input_pos++;
                uart_write_bytes(UART_NUM_0, &c, 1);
            }
        }
        
        if (g_session.logged_in) {
            g_session.last_activity = esp_timer_get_time() / 1000;
        }
    }
    
    return STATUS_OK;
}

// ============================================================================
// ОТПРАВКА СТРОКИ
// ============================================================================
Status_t bbs_ansi_print(const char* text)
{
    if (!text) {
        return STATUS_INVALID;
    }
    
    uart_write_bytes(UART_NUM_0, text, strlen(text));
    
    return STATUS_OK;
}

// ============================================================================
// ОТПРАВКА С ЦВЕТОМ
// ============================================================================
Status_t bbs_ansi_print_color(const char* text, const char* color)
{
    bbs_ansi_print(color);
    bbs_ansi_print(text);
    bbs_ansi_print(ANSI_RESET);
    
    return STATUS_OK;
}

// ============================================================================
// ОЧИСТКА ЭКРАНА
// ============================================================================
Status_t bbs_ansi_clear(void)
{
    return bbs_ansi_print(ANSI_CLEAR);
}

// ============================================================================
// ЭКРАН ВХОДА
// ============================================================================
Status_t bbs_ansi_login_screen(void)
{
    bbs_ansi_clear();
    
    bbs_ansi_print_color("========================================\r\n", ANSI_CYAN);
    bbs_ansi_print_color("       LoRa FidoNet BBS Server\r\n", ANSI_YELLOW);
    bbs_ansi_print_color("========================================\r\n", ANSI_CYAN);
    bbs_ansi_print("\r\n");
    bbs_ansi_print_color("Node: ", ANSI_GREEN);
    bbs_ansi_print(NODE_NAME);
    bbs_ansi_print("\r\n");
    bbs_ansi_print_color("ID: ", ANSI_GREEN);
    
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%d", MY_NODE_ID);
    bbs_ansi_print(id_str);
    bbs_ansi_print("\r\n\r\n");
    
    bbs_ansi_print_color("Enter your name: ", ANSI_WHITE);
    
    g_session.state = BBS_STATE_LOGIN;
    
    return STATUS_OK;
}

// ============================================================================
// ГЛАВНОЕ МЕНЮ
// ============================================================================
Status_t bbs_ansi_show_menu(void)
{
    bbs_ansi_clear();
    
    bbs_ansi_print_color("========================================\r\n", ANSI_CYAN);
    bbs_ansi_print_color("           Main Menu\r\n", ANSI_YELLOW);
    bbs_ansi_print_color("========================================\r\n", ANSI_CYAN);
    bbs_ansi_print("\r\n");
    
    bbs_ansi_print_color("  [1] ", ANSI_GREEN);
    bbs_ansi_print("NetMail\r\n");
    bbs_ansi_print_color("  [2] ", ANSI_GREEN);
    bbs_ansi_print("Message Board\r\n");
    bbs_ansi_print_color("  [3] ", ANSI_GREEN);
    bbs_ansi_print("Files\r\n");
    bbs_ansi_print_color("  [4] ", ANSI_GREEN);
    bbs_ansi_print("Nodes\r\n");
    bbs_ansi_print_color("  [5] ", ANSI_GREEN);
    bbs_ansi_print("SysOp\r\n");
    bbs_ansi_print_color("  [0] ", ANSI_RED);
    bbs_ansi_print("Logout\r\n");
    bbs_ansi_print("\r\n");
    bbs_ansi_print_color("Select: ", ANSI_WHITE);
    
    g_session.state = BBS_STATE_MENU;
    
    return STATUS_OK;
}

// ============================================================================
// ОБРАБОТКА КОМАНДЫ
// ============================================================================
Status_t bbs_ansi_handle_command(char cmd)
{
    switch (g_session.state) {
        case BBS_STATE_LOGIN:
            if (strlen(g_input_buffer) > 0) {
                strncpy(g_session.username, g_input_buffer, sizeof(g_session.username) - 1);
                g_session.logged_in = true;
                g_session.login_time = esp_timer_get_time() / 1000;
                g_session.last_activity = g_session.login_time;
                bbs_ansi_show_menu();
            }
            break;
            
        case BBS_STATE_MENU:
            switch (cmd) {
                case '1':
                    bbs_ansi_print("\r\nNetMail - ");
                    bbs_ansi_print_color("Coming Soon\r\n", ANSI_YELLOW);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    bbs_ansi_show_menu();
                    break;
                case '2':
                    bbs_ansi_print("\r\nMessage Board - ");
                    bbs_ansi_print_color("Coming Soon\r\n", ANSI_YELLOW);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    bbs_ansi_show_menu();
                    break;
                case '3':
                    bbs_ansi_print("\r\nFiles - ");
                    bbs_ansi_print_color("Coming Soon\r\n", ANSI_YELLOW);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    bbs_ansi_show_menu();
                    break;
                case '4':
                    bbs_ansi_print("\r\nNodes: ");
                    {
                        char count_str[16];
                        uint8_t count = routing_get_node_count();
                        snprintf(count_str, sizeof(count_str), "%d", count);
                        bbs_ansi_print(count_str);
                        bbs_ansi_print("\r\n");
                    }
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    bbs_ansi_show_menu();
                    break;
                case '5':
                    bbs_ansi_print("\r\nSysOp Panel - ");
                    bbs_ansi_print_color("Coming Soon\r\n", ANSI_YELLOW);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    bbs_ansi_show_menu();
                    break;
                case '0':
                    bbs_ansi_logout();
                    break;
                default:
                    bbs_ansi_print("\r\nInvalid option\r\n");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    bbs_ansi_show_menu();
                    break;
            }
            break;
            
        default:
            break;
    }
    
    return STATUS_OK;
}

// ============================================================================
// ПОЛУЧЕНИЕ СЕССИИ
// ============================================================================
BbsSession_t* bbs_ansi_get_session(void)
{
    return &g_session;
}

// ============================================================================
// ВЫХОД
// ============================================================================
Status_t bbs_ansi_logout(void)
{
    bbs_ansi_print("\r\n\r\nGoodbye!\r\n");
    
    memset(&g_session, 0, sizeof(g_session));
    g_session.state = BBS_STATE_LOGIN;
    g_input_pos = 0;
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    bbs_ansi_login_screen();
    
    return STATUS_OK;
}