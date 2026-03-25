#pragma once

#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ANSI КОДЫ
// ============================================================================
#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_DIM        "\033[2m"
#define ANSI_ITALIC     "\033[3m"
#define ANSI_UNDERLINE  "\033[4m"
#define ANSI_BLINK      "\033[5m"
#define ANSI_REVERSE    "\033[7m"
#define ANSI_HIDDEN     "\033[8m"

// Цвета текста
#define ANSI_BLACK      "\033[30m"
#define ANSI_RED        "\033[31m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_BLUE       "\033[34m"
#define ANSI_MAGENTA    "\033[35m"
#define ANSI_CYAN       "\033[36m"
#define ANSI_WHITE      "\033[37m"

// Цвета фона
#define ANSI_BG_BLACK   "\033[40m"
#define ANSI_BG_RED     "\033[41m"
#define ANSI_BG_GREEN   "\033[42m"
#define ANSI_BG_YELLOW  "\033[43m"
#define ANSI_BG_BLUE    "\033[44m"
#define ANSI_BG_MAGENTA "\033[45m"
#define ANSI_BG_CYAN    "\033[46m"
#define ANSI_BG_WHITE   "\033[47m"

// Управление курсором
#define ANSI_HOME       "\033[H"
#define ANSI_CLEAR      "\033[2J"
#define ANSI_CLS        "\033[2J\033[H"

// ============================================================================
// ФУНКЦИИ
// ============================================================================

Status_t ui_init(void);
void ui_clear(void);
void ui_gotoxy(uint8_t x, uint8_t y);

// Печать
void ui_print(const char* str);
void ui_print_line(const char* str);
void ui_print_color(const char* str, const char* color);
void ui_print_at(uint8_t x, uint8_t y, const char* str);
void ui_print_color_at(uint8_t x, uint8_t y, const char* str, const char* color);

// BBS-специфичные
void ui_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char* title);
void ui_draw_line(uint8_t x, uint8_t y, uint8_t w, const char* style);
void ui_draw_status_bar(const char* left, const char* center, const char* right);
void ui_draw_main_menu(void);
void ui_draw_login_screen(void);
void ui_draw_mail_screen(void);
void ui_draw_board_screen(void);
void ui_draw_files_screen(void);
void ui_draw_nodes_screen(void);

// Ввод
char ui_get_char(void);
void ui_get_line(char* buf, int max);
char ui_get_hotkey(void);

// Утилиты
void ui_delay_ms(uint32_t ms);
void ui_beep(void);
void ui_flash_line(uint8_t y, uint32_t times);

#ifdef __cplusplus
}
#endif