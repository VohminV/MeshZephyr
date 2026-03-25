#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ANSI КОДЫ
// ============================================================================
#define ANSI_CLEAR          "\033[2J\033[H"
#define ANSI_HOME           "\033[H"
#define ANSI_RESET          "\033[0m"
#define ANSI_BOLD           "\033[1m"
#define ANSI_DIM            "\033[2m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_WHITE          "\033[37m"
#define ANSI_BG_BLACK       "\033[40m"
#define ANSI_BG_BLUE        "\033[44m"

// ============================================================================
// СОСТОЯНИЕ BBS
// ============================================================================
typedef enum {
    BBS_STATE_LOGIN,
    BBS_STATE_MENU,
    BBS_STATE_MAIL,
    BBS_STATE_BOARD,
    BBS_STATE_FILES,
    BBS_STATE_CHAT,
    BBS_STATE_SYSOP
} BbsState_t;

typedef struct {
    BbsState_t state;
    uint16_t   user_id;
    char       username[MAX_USERNAME_LEN];
    uint32_t   login_time;
    uint32_t   last_activity;
    bool       logged_in;
} BbsSession_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация BBS подсистемы
Status_t bbs_ansi_init(void);

// Обработка ввода пользователя
Status_t bbs_ansi_process(void);

// Отправка строки с ANSI кодами
Status_t bbs_ansi_print(const char* text);

// Отправка строки с цветом
Status_t bbs_ansi_print_color(const char* text, const char* color);

// Очистка экрана
Status_t bbs_ansi_clear(void);

// Показ главного меню
Status_t bbs_ansi_show_menu(void);

// Экран входа
Status_t bbs_ansi_login_screen(void);

// Обработка команды
Status_t bbs_ansi_handle_command(char cmd);

// Получение текущей сессии
BbsSession_t* bbs_ansi_get_session(void);

// Завершение сессии
Status_t bbs_ansi_logout(void);

#ifdef __cplusplus
}
#endif