#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"

#include "config.h"
#include "store_forward.h"
#include "lora_mesh.h"
#include "routing.h"

static const char* TAG = "BBS_STORE";

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static StoreForward_t g_store_forward;
static bool g_initialized = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ STORE & FORWARD
// ============================================================================
Status_t store_forward_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing Store & Forward subsystem...");
    
    memset(&g_store_forward, 0, sizeof(g_store_forward));
    g_store_forward.last_process = esp_timer_get_time() / 1000;
    
    store_forward_load_from_fs();
    
    g_initialized = true;
    ESP_LOGI(TAG, "Store & Forward initialized with %d packets", g_store_forward.count);
    
    return STATUS_OK;
}

// ============================================================================
// ДОБАВЛЕНИЕ ПАКЕТА В ОЧЕРЕДЬ
// ============================================================================
Status_t store_forward_enqueue(const FullPacket_t* pkt, uint16_t next_hop)
{
    if (!pkt || g_store_forward.count >= MAX_STORED_PACKETS) {
        return STATUS_QUEUE_FULL;
    }
    
    for (uint8_t i = 0; i < g_store_forward.count; i++) {
        if (g_store_forward.packets[i].packet.header.src == pkt->header.src &&
            g_store_forward.packets[i].packet.header.seq == pkt->header.seq) {
            DBG_LOG(TAG, "Duplicate packet ignored");
            return STATUS_OK;
        }
    }
    
    StoredPacket_t* stored = &g_store_forward.packets[g_store_forward.count];
    memcpy(&stored->packet, pkt, sizeof(FullPacket_t));
    stored->stored_at = esp_timer_get_time() / 1000;
    stored->last_retry = stored->stored_at;
    stored->retries = 0;
    stored->state = 0;
    stored->next_hop = next_hop;
    
    g_store_forward.count++;
    
    DBG_LOG(TAG, "Packet stored: src=%d seq=%d", pkt->header.src, pkt->header.seq);
    
    return STATUS_OK;
}

// ============================================================================
// ОБРАБОТКА ОЧЕРЕДИ
// ============================================================================
Status_t store_forward_process(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - g_store_forward.last_process < 1000) {
        return STATUS_BUSY;
    }
    
    g_store_forward.last_process = now;
    
    for (uint8_t i = 0; i < g_store_forward.count; i++) {
        StoredPacket_t* stored = &g_store_forward.packets[i];
        
        if (stored->state != 0) {
            continue;
        }
        
        if (now - stored->stored_at > STORE_TIMEOUT) {
            DBG_LOG(TAG, "Packet expired, removing");
            store_forward_remove(stored->packet.header.src, stored->packet.header.seq);
            continue;
        }
        
        if (stored->retries >= MAX_RETRIES) {
            stored->state = 3;
            DBG_LOG(TAG, "Packet failed after %d retries", stored->retries);
            continue;
        }
        
        if (now - stored->last_retry > RETRY_TIMEOUT) {
            Status_t status = lora_mesh_send(&stored->packet);
            
            if (status == STATUS_OK) {
                stored->state = 1;
                stored->last_retry = now;
                DBG_LOG(TAG, "Packet sent, waiting for ACK");
            } else {
                stored->retries++;
                stored->last_retry = now;
                DBG_WARN(TAG, "Send failed, retry %d", stored->retries);
            }
        }
    }
    
    return STATUS_OK;
}

// ============================================================================
// ПОИСК ПАКЕТА
// ============================================================================
StoredPacket_t* store_forward_find(uint16_t src, uint16_t seq)
{
    for (uint8_t i = 0; i < g_store_forward.count; i++) {
        if (g_store_forward.packets[i].packet.header.src == src &&
            g_store_forward.packets[i].packet.header.seq == seq) {
            return &g_store_forward.packets[i];
        }
    }
    
    return NULL;
}

// ============================================================================
// УДАЛЕНИЕ ПАКЕТА
// ============================================================================
Status_t store_forward_remove(uint16_t src, uint16_t seq)
{
    for (uint8_t i = 0; i < g_store_forward.count; i++) {
        if (g_store_forward.packets[i].packet.header.src == src &&
            g_store_forward.packets[i].packet.header.seq == seq) {
            
            for (uint8_t j = i; j < g_store_forward.count - 1; j++) {
                g_store_forward.packets[j] = g_store_forward.packets[j + 1];
            }
            g_store_forward.count--;
            
            DBG_LOG(TAG, "Packet removed: src=%d seq=%d", src, seq);
            return STATUS_OK;
        }
    }
    
    return STATUS_NOT_FOUND;
}

// ============================================================================
// ПОДТВЕРЖДЕНИЕ ПОЛУЧЕНИЯ
// ============================================================================
Status_t store_forward_ack(uint16_t src, uint16_t seq)
{
    StoredPacket_t* stored = store_forward_find(src, seq);
    
    if (stored) {
        stored->state = 2;
        store_forward_remove(src, seq);
        DBG_LOG(TAG, "Packet acknowledged: src=%d seq=%d", src, seq);
        return STATUS_OK;
    }
    
    return STATUS_NOT_FOUND;
}

// ============================================================================
// ОЧИСТКА
// ============================================================================
void store_forward_cleanup(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    uint8_t new_count = 0;
    
    for (uint8_t i = 0; i < g_store_forward.count; i++) {
        if (now - g_store_forward.packets[i].stored_at < STORE_TIMEOUT &&
            g_store_forward.packets[i].state != 3) {
            g_store_forward.packets[new_count] = g_store_forward.packets[i];
            new_count++;
        }
    }
    
    g_store_forward.count = new_count;
    DBG_LOG(TAG, "Store & Forward cleaned: %d packets", new_count);
}

// ============================================================================
// СТАТИСТИКА
// ============================================================================
uint8_t store_forward_get_count(void)
{
    return g_store_forward.count;
}

uint8_t store_forward_get_pending_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < g_store_forward.count; i++) {
        if (g_store_forward.packets[i].state == 0) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// ЗАГРУЗКА ИЗ ФС
// ============================================================================
Status_t store_forward_load_from_fs(void)
{
    char path[64];
    snprintf(path, sizeof(path), "%s/queue.dat", CONFIG_DIR);
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        return STATUS_NOT_FOUND;
    }
    
    size_t read = fread(&g_store_forward, 1, sizeof(g_store_forward), f);
    fclose(f);
    
    if (read < sizeof(StoreForward_t)) {
        return STATUS_INVALID;
    }
    
    DBG_LOG(TAG, "Queue loaded from filesystem");
    
    return STATUS_OK;
}

// ============================================================================
// СОХРАНЕНИЕ В ФС
// ============================================================================
Status_t store_forward_save_to_fs(void)
{
    char path[64];
    snprintf(path, sizeof(path), "%s/queue.dat", CONFIG_DIR);
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        return STATUS_FS_ERROR;
    }
    
    size_t written = fwrite(&g_store_forward, 1, sizeof(g_store_forward), f);
    fclose(f);
    
    if (written != sizeof(StoreForward_t)) {
        return STATUS_FS_ERROR;
    }
    
    DBG_LOG(TAG, "Queue saved to filesystem");
    
    return STATUS_OK;
}