#pragma once

#include "config.h"
#include "packet.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ХРАНИЛИЩЕ ПАКЕТОВ
// ============================================================================
typedef struct {
    FullPacket_t   packet;
    uint32_t       stored_at;
    uint32_t       last_retry;
    uint8_t        retries;
    uint8_t        state;  // 0=pending, 1=sending, 2=sent, 3=failed
    uint16_t       next_hop;
} StoredPacket_t;

typedef struct {
    StoredPacket_t packets[MAX_STORED_PACKETS];
    uint8_t        count;
    uint32_t       last_process;
} StoreForward_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация подсистемы Store & Forward
Status_t store_forward_init(void);

// Сохранение пакета в очередь
Status_t store_forward_enqueue(const FullPacket_t* pkt, uint16_t next_hop);

// Обработка очереди
Status_t store_forward_process(void);

// Поиск пакета по ID
StoredPacket_t* store_forward_find(uint16_t src, uint16_t seq);

// Удаление пакета из очереди
Status_t store_forward_remove(uint16_t src, uint16_t seq);

// Подтверждение получения пакета
Status_t store_forward_ack(uint16_t src, uint16_t seq);

// Очистка устаревших пакетов
void store_forward_cleanup(void);

// Получение статистики
uint8_t store_forward_get_count(void);
uint8_t store_forward_get_pending_count(void);

// Загрузка из файловой системы
Status_t store_forward_load_from_fs(void);

// Сохранение в файловую систему
Status_t store_forward_save_to_fs(void);

#ifdef __cplusplus
}
#endif