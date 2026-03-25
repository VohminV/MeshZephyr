#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"

#include "config.h"
#include "file_transfer.h"
#include "packet.h"
#include "lora_mesh.h"
#include "routing.h"

static const char* TAG = TAG_FILE;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static FileTransferManager_t g_file_manager;
static bool g_initialized = false;

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ СОЗДАНИЯ ДИРЕКТОРИИ
// ============================================================================
static Status_t create_directory(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return STATUS_OK;
        }
    }
    
    if (mkdir(path, 0755) != 0) {
        ESP_LOGW(TAG, "Failed to create directory: %s", path);
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Directory created: %s", path);
    return STATUS_OK;
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ПЕРЕДАЧИ ФАЙЛОВ
// ============================================================================
Status_t file_transfer_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing File Transfer subsystem...");
    
    memset(&g_file_manager, 0, sizeof(g_file_manager));
    g_file_manager.next_file_id = 1;
    
    create_directory(FILES_DIR);
    create_directory(MAIL_IN_DIR);
    create_directory(MAIL_OUT_DIR);
    create_directory(BOARD_DIR);
    create_directory(CONFIG_DIR);
    create_directory(KEYS_DIR);
    
    g_initialized = true;
    ESP_LOGI(TAG, "File Transfer initialized");
    
    return STATUS_OK;
}

// ============================================================================
// НАЧАЛО ПЕРЕДАЧИ ФАЙЛА
// ============================================================================
Status_t file_transfer_start(const char* filename, uint16_t receiver)
{
    if (!filename) {
        return STATUS_INVALID;
    }
    
    if (g_file_manager.count >= MAX_FILE_QUEUE) {
        return STATUS_QUEUE_FULL;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", FILES_DIR, filename);
    
    FILE* f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "File not found: %s", path);
        return STATUS_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    FileInfo_t* file = &g_file_manager.files[g_file_manager.count];
    memset(file, 0, sizeof(FileInfo_t));
    
    file->file_id = g_file_manager.next_file_id++;
    strncpy(file->filename, filename, sizeof(file->filename) - 1);
    file->size = file_size;
    file->sender = MY_NODE_ID;
    file->receiver = receiver;
    file->timestamp = esp_timer_get_time() / 1000;
    file->total_fragments = (file_size + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    file->received_fragments = 0;
    file->status = 0;
    file->retries = 0;
    
    uint8_t buffer[MAX_PAYLOAD_SIZE];
    uint32_t crc = 0;
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        crc = file_transfer_calc_crc32(buffer, bytes_read);
    }
    file->crc32 = crc;
    
    fclose(f);
    g_file_manager.count++;
    
    FileFragment_t frag;
    memset(&frag, 0, sizeof(frag));
    frag.file_id = file->file_id;
    frag.offset = 0;
    frag.total_size = file_size;
    frag.fragment_num = 0;
    frag.total_fragments = file->total_fragments;
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_FILE_INFO, MY_NODE_ID, receiver, sizeof(FileFragment_t));
    pkt.header.flags = PKT_FLAG_SIGNED | PKT_FLAG_ACK_REQ;
    memcpy(pkt.payload, &frag, sizeof(FileFragment_t));
    pkt.header.payload_len = sizeof(FileFragment_t);
    
    Status_t status = lora_mesh_send(&pkt);
    
    if (status == STATUS_OK) {
        file->status = 1;
        ESP_LOGI(TAG, "File transfer started: %s (%lu bytes)", filename, file_size);
    }
    
    return status;
}

// ============================================================================
// ПРИЕМ ИНФОРМАЦИИ О ФАЙЛЕ
// ============================================================================
Status_t file_transfer_receive_info(const FileFragment_t* frag)
{
    if (!frag) {
        return STATUS_INVALID;
    }
    
    if (g_file_manager.count >= MAX_FILE_QUEUE) {
        return STATUS_QUEUE_FULL;
    }
    
    FileInfo_t* file = &g_file_manager.files[g_file_manager.count];
    memset(file, 0, sizeof(FileInfo_t));
    
    file->file_id = frag->file_id;
    file->size = frag->total_size;
    file->total_fragments = frag->total_fragments;
    file->received_fragments = 0;
    file->status = 1;
    file->timestamp = esp_timer_get_time() / 1000;
    
    g_file_manager.count++;
    
    ESP_LOGI(TAG, "File info received: id=%lu size=%lu fragments=%d", 
            (unsigned long)frag->file_id, (unsigned long)frag->total_size, frag->total_fragments);
    
    return STATUS_OK;
}

// ============================================================================
// ПРИЕМ ФРАГМЕНТА ФАЙЛА
// ============================================================================
Status_t file_transfer_receive_data(const FileFragment_t* frag)
{
    if (!frag) {
        return STATUS_INVALID;
    }
    
    FileInfo_t* file = NULL;
    for (uint8_t i = 0; i < g_file_manager.count; i++) {
        if (g_file_manager.files[i].file_id == frag->file_id) {
            file = &g_file_manager.files[i];
            break;
        }
    }
    
    if (!file) {
        ESP_LOGW(TAG, "File not found: %lu", (unsigned long)frag->file_id);
        return STATUS_NOT_FOUND;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.dat", FILES_DIR, (unsigned long)frag->file_id);
    
    FILE* f = fopen(path, "r+b");
    if (!f) {
        f = fopen(path, "wb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to create file: %s", path);
            return STATUS_FS_ERROR;
        }
    }
    
    fseek(f, frag->offset, SEEK_SET);
    fwrite(frag->data, 1, sizeof(frag->data), f);
    fclose(f);
    
    file->received_fragments++;
    
    ESP_LOGI(TAG, "Fragment received: %d/%d", file->received_fragments, file->total_fragments);
    
    if (file->received_fragments >= file->total_fragments) {
        file->status = 2;
        ESP_LOGI(TAG, "File transfer complete: %lu", (unsigned long)frag->file_id);
    }
    
    file_transfer_send_ack(MY_NODE_ID, frag->file_id, frag->fragment_num);
    
    return STATUS_OK;
}

// ============================================================================
// ОТПРАВКА ПОДТВЕРЖДЕНИЯ
// ============================================================================
Status_t file_transfer_send_ack(uint16_t sender, uint32_t file_id, uint16_t fragment_num)
{
    FullPacket_t pkt;
    
    packet_init_header(&pkt.header, PKT_FILE_ACK, MY_NODE_ID, sender, 8);
    pkt.header.flags = PKT_FLAG_SIGNED;
    
    uint8_t* payload = pkt.payload;
    memcpy(payload, &file_id, 4);
    memcpy(payload + 4, &fragment_num, 2);
    pkt.header.payload_len = 6;
    
    return lora_mesh_send(&pkt);
}

// ============================================================================
// ПРОВЕРКА ЦЕЛОСТНОСТИ
// ============================================================================
Status_t file_transfer_verify(uint32_t file_id)
{
    FileInfo_t* file = NULL;
    for (uint8_t i = 0; i < g_file_manager.count; i++) {
        if (g_file_manager.files[i].file_id == file_id) {
            file = &g_file_manager.files[i];
            break;
        }
    }
    
    if (!file || file->status != 2) {
        return STATUS_NOT_FOUND;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.dat", FILES_DIR, (unsigned long)file_id);
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        return STATUS_FS_ERROR;
    }
    
    uint8_t buffer[MAX_PAYLOAD_SIZE];
    uint32_t crc = 0;
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        crc = file_transfer_calc_crc32(buffer, bytes_read);
    }
    
    fclose(f);
    
    if (crc != file->crc32) {
        file->status = 3;
        return STATUS_INVALID;
    }
    
    return STATUS_OK;
}

// ============================================================================
// ОТМЕНА ПЕРЕДАЧИ
// ============================================================================
Status_t file_transfer_cancel(uint32_t file_id)
{
    for (uint8_t i = 0; i < g_file_manager.count; i++) {
        if (g_file_manager.files[i].file_id == file_id) {
            g_file_manager.files[i].status = 3;
            
            char path[128];
            snprintf(path, sizeof(path), "%s/%lu.dat", FILES_DIR, (unsigned long)file_id);
            remove(path);
            
            ESP_LOGI(TAG, "File transfer cancelled: %lu", (unsigned long)file_id);
            return STATUS_OK;
        }
    }
    
    return STATUS_NOT_FOUND;
}

// ============================================================================
// ПОЛУЧЕНИЕ КОЛИЧЕСТВА ФАЙЛОВ
// ============================================================================
uint8_t file_transfer_get_count(void)
{
    return g_file_manager.count;
}

// ============================================================================
// СПИСОК ФАЙЛОВ
// ============================================================================
Status_t file_transfer_list(FileInfo_t* list, uint8_t max_count)
{
    if (!list) {
        return STATUS_INVALID;
    }
    
    uint8_t copied = 0;
    for (uint8_t i = 0; i < g_file_manager.count && copied < max_count; i++) {
        list[copied] = g_file_manager.files[i];
        copied++;
    }
    
    return STATUS_OK;
}

// ============================================================================
// ОЧИСТКА ЗАВЕРШЕННЫХ ПЕРЕДАЧ
// ============================================================================
void file_transfer_cleanup(void)
{
    uint8_t new_count = 0;
    
    for (uint8_t i = 0; i < g_file_manager.count; i++) {
        if (g_file_manager.files[i].status != 2 && 
            g_file_manager.files[i].status != 3) {
            g_file_manager.files[new_count] = g_file_manager.files[i];
            new_count++;
        }
    }
    
    g_file_manager.count = new_count;
}

// ============================================================================
// РАСЧЕТ CRC32
// ============================================================================
uint32_t file_transfer_calc_crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}