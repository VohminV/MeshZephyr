#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// КОМАНДЫ SYSOP
// ============================================================================
typedef enum {
    SYSOP_CMD_STATUS      = 0,
    SYSOP_CMD_ROUTES      = 1,
    SYSOP_CMD_NODES       = 2,
    SYSOP_CMD_MAIL_QUEUE  = 3,
    SYSOP_CMD_FILE_QUEUE  = 4,
    SYSOP_CMD_STORED      = 5,
    SYSOP_CMD_REBOOT      = 6,
    SYSOP_CMD_RESET       = 7,
    SYSOP_CMD_CONFIG      = 8,
    SYSOP_CMD_USERS       = 9,
    SYSOP_CMD_LOGS        = 10,
    SYSOP_CMD_TEST        = 11
} SysOpCommand_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация SysOp подсистемы
Status_t sysop_init(void);

// Обработка удаленной команды
Status_t sysop_handle_remote_cmd(uint16_t from, const uint8_t* cmd, size_t len);

// Отправка статуса узла
Status_t sysop_send_status(uint16_t to);

// Отправка таблицы маршрутов
Status_t sysop_send_routes(uint16_t to);

// Отправка списка узлов
Status_t sysop_send_nodes(uint16_t to);

// Отправка логов
Status_t sysop_send_logs(uint16_t to, uint16_t lines);

// Выполнение локальной команды
Status_t sysop_execute_command(SysOpCommand_t cmd, const char* args);

// Проверка прав SysOp
bool sysop_check_privileges(uint16_t user_id);

#ifdef __cplusplus
}
#endif