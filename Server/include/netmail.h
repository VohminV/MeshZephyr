#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// СТРУКТУРА ПИСЬМА
// ============================================================================
typedef struct {
    uint32_t id;
    uint16_t from;
    uint16_t to;
    char     subject[64];
    char     from_name[MAX_USERNAME_LEN];
    char     to_name[MAX_USERNAME_LEN];
    uint32_t timestamp;
    uint32_t size;
    uint8_t  flags;
    uint8_t  status;  // 0=new, 1=read, 2=sent, 3=failed
} NetMail_t;

#define MAIL_FLAG_READ      0x01
#define MAIL_FLAG_PRIORITY  0x02
#define MAIL_FLAG_ENCRYPTED 0x04

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация почтовой системы
Status_t netmail_init(void);

// Создание нового письма
Status_t netmail_create(NetMail_t* mail, uint16_t to, const char* subject);

// Сохранение письма в файловую систему
Status_t netmail_save(const NetMail_t* mail, const char* body);

// Загрузка письма из файловой системы
Status_t netmail_load(uint32_t id, NetMail_t* mail, char* body, size_t body_size);

// Добавление письма во входящие
Status_t netmail_add_incoming(const NetMail_t* mail, const char* body);

// Постановка письма в очередь отправки
Status_t netmail_queue_outgoing(uint32_t mail_id);

// Обработка очереди отправки
Status_t netmail_process_queue(void);

// Удаление письма
Status_t netmail_delete(uint32_t id);

// Получение списка писем
uint16_t netmail_get_inbox_count(void);
uint16_t netmail_get_outbox_count(void);
Status_t netmail_list_inbox(NetMail_t* list, uint16_t max_count);

// Поиск письма
NetMail_t* netmail_find(uint32_t id);

// Очистка старых писем
void netmail_cleanup(void);

#ifdef __cplusplus
}
#endif