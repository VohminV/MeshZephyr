#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "ui_terminal.h"


// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
Status_t ui_init(void)
{
    uart_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.baud_rate = 115200;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &cfg);
    
    ui_clear();
    return STATUS_OK;
}

void ui_clear(void)
{
    uart_write_bytes(UART_NUM_0, ANSI_CLS, strlen(ANSI_CLS));
}

void ui_gotoxy(uint8_t x, uint8_t y)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
    uart_write_bytes(UART_NUM_0, buf, strlen(buf));
}

// ============================================================================
// ПЕЧАТЬ
// ============================================================================
void ui_print(const char* str)
{
    if (!str) return;
    uart_write_bytes(UART_NUM_0, str, strlen(str));
}

void ui_print_line(const char* str)
{
    ui_print(str);
    ui_print("\r\n");
}

void ui_print_color(const char* str, const char* color)
{
    ui_print(color);
    ui_print(str);
    ui_print(ANSI_RESET);
}

void ui_print_at(uint8_t x, uint8_t y, const char* str)
{
    ui_gotoxy(x, y);
    ui_print(str);
}

void ui_print_color_at(uint8_t x, uint8_t y, const char* str, const char* color)
{
    ui_gotoxy(x, y);
    ui_print_color(str, color);
}

// ============================================================================
// РИСОВАНИЕ ЭЛЕМЕНТОВ
// ============================================================================
void ui_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char* title)
{
    // Верхняя рамка
    ui_gotoxy(x, y);
    ui_print_color("╔", ANSI_CYAN);
    for (uint8_t i = 0; i < w - 2; i++) ui_print_color("═", ANSI_CYAN);
    ui_print_color("╗", ANSI_CYAN);
    
    // Заголовок
    if (title && strlen(title) > 0) {
        ui_gotoxy(x + (w - strlen(title)) / 2, y);
        ui_print_color(" ", ANSI_CYAN);
        ui_print_color(title, ANSI_YELLOW ANSI_BOLD);
        ui_print_color(" ", ANSI_CYAN);
    }
    
    // Боковые рамки
    for (uint8_t row = 1; row < h - 1; row++) {
        ui_gotoxy(x, y + row);
        ui_print_color("║", ANSI_CYAN);
        ui_gotoxy(x + w - 1, y + row);
        ui_print_color("║", ANSI_CYAN);
    }
    
    // Нижняя рамка
    ui_gotoxy(x, y + h - 1);
    ui_print_color("╚", ANSI_CYAN);
    for (uint8_t i = 0; i < w - 2; i++) ui_print_color("═", ANSI_CYAN);
    ui_print_color("╝", ANSI_CYAN);
}

void ui_draw_line(uint8_t x, uint8_t y, uint8_t w, const char* style)
{
    ui_gotoxy(x, y);
    const char* char_style = strcmp(style, "double") == 0 ? "═" : "─";
    const char* color = strcmp(style, "double") == 0 ? ANSI_CYAN : ANSI_BLUE;
    
    for (uint8_t i = 0; i < w; i++) {
        ui_print_color(char_style, color);
    }
}

void ui_draw_status_bar(const char* left, const char* center, const char* right)
{
    ui_gotoxy(0, 24);
    ui_print(ANSI_BG_BLUE ANSI_WHITE);
    
    // Очистка строки
    for (int i = 0; i < 80; i++) ui_print(" ");
    
    // Контент
    ui_gotoxy(1, 24);
    ui_print(ANSI_BG_BLUE ANSI_WHITE);
    if (left) ui_print(left);
    
    if (center) {
        ui_gotoxy(40 - strlen(center) / 2, 24);
        ui_print(center);
    }
    
    if (right) {
        ui_gotoxy(79 - strlen(right), 24);
        ui_print(right);
    }
    
    ui_print(ANSI_RESET);
}

// ============================================================================
// BBS ЭКРАНЫ
// ============================================================================
void ui_draw_login_screen(void)
{
    ui_clear();
    
    // Заголовок
    ui_gotoxy(25, 2);
    ui_print_color("╔══════════════════════════════════════════╗", ANSI_CYAN ANSI_BOLD);
    ui_gotoxy(25, 3);
    ui_print_color("║", ANSI_CYAN);
    ui_print_color("      MESH ZEPHYR BBS SYSTEM      ", ANSI_YELLOW ANSI_BOLD);
    ui_print_color("║", ANSI_CYAN);
    ui_gotoxy(25, 4);
    ui_print_color("╚══════════════════════════════════════════╝", ANSI_CYAN ANSI_BOLD);
    
    // Рамка входа
    ui_draw_box(20, 8, 40, 10, " LOGIN ");
    
    ui_print_color_at(25, 10, "Node ID:", ANSI_GREEN);
    ui_print_color_at(25, 12, "Name:", ANSI_GREEN);
    ui_print_color_at(25, 14, "Password:", ANSI_GREEN);
    
    ui_draw_status_bar("F1-Help", "Welcome to MeshZephyr", "v1.0");
}

void ui_draw_main_menu(void)
{
    ui_clear();
    
    // Главный заголовок
    ui_gotoxy(10, 1);
    ui_print_color("╔══════════════════════════════════════════════════════════════════════════════╗", ANSI_CYAN ANSI_BOLD);
    ui_gotoxy(10, 2);
    ui_print_color("║", ANSI_CYAN);
    ui_print_color("                          MESH ZEPHYR BBS - MAIN MENU                          ", ANSI_YELLOW ANSI_BOLD);
    ui_print_color("║", ANSI_CYAN);
    ui_gotoxy(10, 3);
    ui_print_color("╚══════════════════════════════════════════════════════════════════════════════╝", ANSI_CYAN ANSI_BOLD);
    
    // Левая колонка - Сканирование
    ui_draw_box(10, 5, 35, 10, " SCAN ");
    ui_print_color_at(12, 6, "[", ANSI_WHITE);
    ui_print_color_at(13, 6, "A", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 6, "] New Message Scan", ANSI_WHITE);
    ui_print_color_at(12, 7, "[", ANSI_WHITE);
    ui_print_color_at(13, 7, "B", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 7, "] All Message Scan", ANSI_WHITE);
    ui_print_color_at(12, 8, "[", ANSI_WHITE);
    ui_print_color_at(13, 8, "C", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 8, "] New File Scan", ANSI_WHITE);
    ui_print_color_at(12, 9, "[", ANSI_WHITE);
    ui_print_color_at(13, 9, "D", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 9, "] All File Scan", ANSI_WHITE);
    
    // Правая колонка - Почта
    ui_draw_box(50, 5, 35, 10, " MAIL ");
    ui_print_color_at(52, 6, "[", ANSI_WHITE);
    ui_print_color_at(53, 6, "R", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 6, "] Read Mail", ANSI_WHITE);
    ui_print_color_at(52, 7, "[", ANSI_WHITE);
    ui_print_color_at(53, 7, "S", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 7, "] Send Mail", ANSI_WHITE);
    ui_print_color_at(52, 8, "[", ANSI_WHITE);
    ui_print_color_at(53, 8, "L", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 8, "] List Mail", ANSI_WHITE);
    ui_print_color_at(52, 9, "[", ANSI_WHITE);
    ui_print_color_at(53, 9, "K", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 9, "] Kill Mail", ANSI_WHITE);
    
    // Средняя колонка - Конференции
    ui_draw_box(10, 16, 35, 10, " CONFERENCES ");
    ui_print_color_at(12, 17, "[", ANSI_WHITE);
    ui_print_color_at(13, 17, "E", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 17, "] Enter Conference", ANSI_WHITE);
    ui_print_color_at(12, 18, "[", ANSI_WHITE);
    ui_print_color_at(13, 18, "X", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 18, "] Extended Commands", ANSI_WHITE);
    ui_print_color_at(12, 19, "[", ANSI_WHITE);
    ui_print_color_at(13, 19, "J", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 19, "] Jump Conference", ANSI_WHITE);
    
    // Правая нижняя - Файлы
    ui_draw_box(50, 16, 35, 10, " FILES ");
    ui_print_color_at(52, 17, "[", ANSI_WHITE);
    ui_print_color_at(53, 17, "F", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 17, "] File Transfer", ANSI_WHITE);
    ui_print_color_at(52, 18, "[", ANSI_WHITE);
    ui_print_color_at(53, 18, "T", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 18, "] Transfer Files", ANSI_WHITE);
    ui_print_color_at(52, 19, "[", ANSI_WHITE);
    ui_print_color_at(53, 19, "V", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(54, 19, "] View Files", ANSI_WHITE);
    
    // Нижняя секция - Разное
    ui_draw_box(10, 27, 20, 5, " MISC ");
    ui_print_color_at(12, 28, "[", ANSI_WHITE);
    ui_print_color_at(13, 28, "U", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 28, "] User Info", ANSI_WHITE);
    ui_print_color_at(12, 29, "[", ANSI_WHITE);
    ui_print_color_at(13, 29, "W", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(14, 29, "] Who's On", ANSI_WHITE);
    
    ui_draw_box(35, 27, 20, 5, " CHAT ");
    ui_print_color_at(37, 28, "[", ANSI_WHITE);
    ui_print_color_at(38, 28, "H", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(39, 28, "] Chat SysOp", ANSI_WHITE);
    ui_print_color_at(37, 29, "[", ANSI_WHITE);
    ui_print_color_at(38, 29, "O", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(39, 29, "] Chat Room", ANSI_WHITE);
    
    ui_draw_box(60, 27, 25, 5, " SYSTEM ");
    ui_print_color_at(62, 28, "[", ANSI_WHITE);
    ui_print_color_at(63, 28, "I", ANSI_YELLOW ANSI_BOLD);
    ui_print_color_at(64, 28, "] System Info", ANSI_WHITE);
    ui_print_color_at(62, 29, "[", ANSI_WHITE);
    ui_print_color_at(63, 29, "Q", ANSI_RED ANSI_BOLD);
    ui_print_color_at(64, 29, "] Quit/Logoff", ANSI_WHITE);
    
    // Статус бар
    char status_line[80];
    snprintf(status_line, sizeof(status_line), " Node: %d | %s ", MY_NODE_ID, NODE_NAME);
    ui_draw_status_bar("F1-Help F10-Main", status_line, "Online");
}

void ui_draw_mail_screen(void)
{
    ui_clear();
    
    ui_draw_box(5, 2, 70, 20, " NETMAIL ");
    
    ui_print_color_at(8, 4, "╔════════════════════════════════════════════════════════════════╗", ANSI_CYAN);
    ui_print_color_at(8, 5, "║ #  From            To              Subject                      ║", ANSI_CYAN);
    ui_print_color_at(8, 6, "╠════════════════════════════════════════════════════════════════╣", ANSI_BLUE);
    ui_print_color_at(8, 7, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 8, "║   No messages in mailbox                                       ║", ANSI_YELLOW);
    ui_print_color_at(8, 9, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 10, "╚════════════════════════════════════════════════════════════════╝", ANSI_CYAN);
    
    ui_print_color_at(10, 15, "[R]ead  [S]end  [D]elete  [Q]uit", ANSI_WHITE);
    ui_print_color_at(10, 16, "[L]ist  [U]ndelete  [P]urge", ANSI_WHITE);
    
    ui_draw_status_bar("Mail Menu", "0 Messages", "ESC-Exit");
}

void ui_draw_board_screen(void)
{
    ui_clear();
    
    ui_draw_box(5, 2, 70, 20, " MESSAGE BOARD ");
    
    ui_print_color_at(8, 4, "╔════════════════════════════════════════════════════════════════╗", ANSI_CYAN);
    ui_print_color_at(8, 5, "║ #  Date       From            Subject                          ║", ANSI_CYAN);
    ui_print_color_at(8, 6, "╠════════════════════════════════════════════════════════════════╣", ANSI_BLUE);
    ui_print_color_at(8, 7, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 8, "║   Board is empty                                               ║", ANSI_YELLOW);
    ui_print_color_at(8, 9, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 10, "╚════════════════════════════════════════════════════════════════╝", ANSI_CYAN);
    
    ui_print_color_at(10, 15, "[R]ead  [P]ost  [D]elete  [S]earch  [Q]uit", ANSI_WHITE);
    
    ui_draw_status_bar("Message Board", "General Discussion", "ESC-Exit");
}

void ui_draw_files_screen(void)
{
    ui_clear();
    
    ui_draw_box(5, 2, 70, 20, " FILE TRANSFER ");
    
    ui_print_color_at(8, 4, "╔════════════════════════════════════════════════════════════════╗", ANSI_CYAN);
    ui_print_color_at(8, 5, "║ Filename          Size    Date        Dir                      ║", ANSI_CYAN);
    ui_print_color_at(8, 6, "╠════════════════════════════════════════════════════════════════╣", ANSI_BLUE);
    ui_print_color_at(8, 7, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 8, "║   No files available                                           ║", ANSI_YELLOW);
    ui_print_color_at(8, 9, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 10, "╚════════════════════════════════════════════════════════════════╝", ANSI_CYAN);
    
    ui_print_color_at(10, 15, "[D]ownload  [U]pload  [L]ist  [S]earch  [Q]uit", ANSI_WHITE);
    
    ui_draw_status_bar("File Area", "Main Directory", "ESC-Exit");
}

void ui_draw_nodes_screen(void)
{
    ui_clear();
    
    ui_draw_box(5, 2, 70, 20, " NETWORK NODES ");
    
    ui_print_color_at(8, 4, "╔════════════════════════════════════════════════════════════════╗", ANSI_CYAN);
    ui_print_color_at(8, 5, "║ ID   Name                    Status      Last Heard            ║", ANSI_CYAN);
    ui_print_color_at(8, 6, "╠════════════════════════════════════════════════════════════════╣", ANSI_BLUE);
    // Сюда будут добавляться узлы динамически
    ui_print_color_at(8, 7, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 8, "║   Scanning for nodes...                                        ║", ANSI_YELLOW);
    ui_print_color_at(8, 9, "║                                                                ║", ANSI_CYAN);
    ui_print_color_at(8, 10, "╚════════════════════════════════════════════════════════════════╝", ANSI_CYAN);
    
    ui_print_color_at(10, 15, "[R]efresh  [P]ing  [I]nfo  [Q]uit", ANSI_WHITE);
    
    ui_draw_status_bar("Network Status", "Scanning...", "ESC-Exit");
}

// ============================================================================
// ВВОД
// ============================================================================
char ui_get_char(void)
{
    uint8_t c;
    while (uart_read_bytes(UART_NUM_0, &c, 1, portMAX_DELAY) == 0);
    return (char)c;
}

void ui_get_line(char* buf, int max)
{
    int i = 0;
    while (i < max - 1) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, portMAX_DELAY)) {
            if (c == '\r' || c == '\n') {
                buf[i] = '\0';
                ui_print("\r\n");
                break;
            } else if (c == 127 || c == '\b') {
                if (i > 0) {
                    i--;
                    ui_print("\b \b");
                }
            } else if (c >= 32 && c < 127) {
                buf[i++] = c;
                uart_write_bytes(UART_NUM_0, &c, 1);
            }
        }
    }
}

char ui_get_hotkey(void)
{
    char c = ui_get_char();
    
    // Обработка F-клавиш (простая эмуляция)
    if (c == 0 || c == -32) {
        c = ui_get_char();
        // F1=59, F10=68 в скан-кодах
    }
    
    return c;
}

// ============================================================================
// УТИЛИТЫ
// ============================================================================
void ui_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void ui_beep(void)
{
    ui_print("\007");
}

void ui_flash_line(uint8_t y, uint32_t times)
{
    for (uint32_t i = 0; i < times; i++) {
        ui_gotoxy(0, y);
        ui_print(ANSI_REVERSE);
        for (int j = 0; j < 80; j++) ui_print(" ");
        ui_delay_ms(100);
        ui_gotoxy(0, y);
        for (int j = 0; j < 80; j++) ui_print(" ");
        ui_delay_ms(100);
    }
    ui_draw_status_bar("", "", "");
}