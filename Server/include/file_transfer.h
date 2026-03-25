#pragma once

#include "config.h"
#include "packet.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ИНФОРМАЦИЯ О ФАЙЛЕ
// ============================================================================
typedef struct {
    uint32_t file_id;
    char     filename[64];
    uint32_t size;
    uint32_t crc32;
    uint16_t sender;
    uint16_t receiver;
    uint32_t timestamp;
    uint16_t total_fragments;
    uint16_t received_fragments;
    uint8_t  status;  // 0=pending, 1=transferring, 2=complete, 3=failed
    uint8_t  retries;
} FileInfo_t;

// ============================================================================
// МЕНЕДЖЕР ПЕРЕДАЧИ
// ============================================================================
typedef struct {
    FileInfo_t files[MAX_FILE_QUEUE];
    uint8_t    count;
    uint32_t   next_file_id;
} FileTransferManager_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация системы передачи файлов
Status_t file_transfer_init(void);

// Начало передачи файла
Status_t file_transfer_start(const char* filename, uint16_t receiver);

// Прием информации о файле
Status_t file_transfer_receive_info(const FileFragment_t* frag);

// Прием фрагмента файла
Status_t file_transfer_receive_data(const FileFragment_t* frag);

// Подтверждение получения фрагмента
Status_t file_transfer_send_ack(uint16_t sender, uint32_t file_id, 
                                uint16_t fragment_num);

// Проверка целостности файла
Status_t file_transfer_verify(uint32_t file_id);

// Отмена передачи
Status_t file_transfer_cancel(uint32_t file_id);

// Получение списка файлов
uint8_t file_transfer_get_count(void);
Status_t file_transfer_list(FileInfo_t* list, uint8_t max_count);

// Очистка завершенных передач
void file_transfer_cleanup(void);

// Расчет CRC32
uint32_t file_transfer_calc_crc32(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif