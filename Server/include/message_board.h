#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// СТРУКТУРА СООБЩЕНИЯ
// ============================================================================
typedef struct {
    uint32_t id;
    uint16_t author;
    char     author_name[MAX_USERNAME_LEN];
    char     subject[64];
    uint32_t timestamp;
    uint32_t size;
    uint8_t  flags;
} BoardMessage_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация доски объявлений
Status_t message_board_init(void);

// Публикация нового сообщения
Status_t message_board_post(uint16_t author, const char* author_name,
                            const char* subject, const char* text);

// Чтение сообщения
Status_t message_board_read(uint32_t id, BoardMessage_t* msg, 
                            char* text, size_t text_size);

// Получение списка сообщений
uint16_t message_board_get_count(void);
Status_t message_board_list(BoardMessage_t* list, uint16_t start, uint16_t count);

// Удаление сообщения (SysOp)
Status_t message_board_delete(uint32_t id);

// Очистка старых сообщений
void message_board_cleanup(void);

// Рассылка нового сообщения по сети
Status_t message_board_broadcast(const BoardMessage_t* msg, const char* text);

#ifdef __cplusplus
}
#endif