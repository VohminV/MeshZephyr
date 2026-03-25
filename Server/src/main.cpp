#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "esp_heap_caps.h"
#include "config.h"
#include "crypto.h"
#include "packet.h"
#include "routing.h"
#include "store_forward.h"
#include "netmail.h"
#include "message_board.h"
#include "file_transfer.h"
#include "bbs_ansi.h"
#include "sysop.h"
#include "lora_mesh.h"


static const char* TAG = TAG_MAIN;

// ============================================================================
// ГЛОБАЛЬНЫЕ ФЛАГИ
// ============================================================================
static bool g_system_ready = false;
static uint32_t g_startup_time = 0;
static uint32_t g_last_route_exchange = 0;
static uint32_t g_last_beacon = 0;
static uint32_t g_last_cleanup = 0;

// ============================================================================
// ЗАДАЧИ
// ============================================================================
static void bbs_task(void* pvParameters);
static void lora_rx_task(void* pvParameters);
static void network_task(void* pvParameters);
static void cleanup_task(void* pvParameters);

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
// ИНИЦИАЛИЗАЦИЯ ФАЙЛОВОЙ СИСТЕМЫ
// ============================================================================
static Status_t init_filesystem(void)
{
    ESP_LOGI(TAG, "Initializing filesystem...");
    
    create_directory(FS_MOUNT_POINT);
    create_directory(MAIL_IN_DIR);
    create_directory(MAIL_OUT_DIR);
    create_directory(MAIL_SENT_DIR);
    create_directory(BOARD_DIR);
    create_directory(FILES_DIR);
    create_directory(CONFIG_DIR);
    create_directory(KEYS_DIR);
    create_directory(LOGS_DIR);
    
    ESP_LOGI(TAG, "Filesystem ready at %s", FS_MOUNT_POINT);
    
    return STATUS_OK;
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ВСЕХ ПОДСИСТЕМ
// ============================================================================
static Status_t init_all_subsystems(void)
{
    Status_t status;
    
    ESP_LOGI(TAG, "Initializing crypto subsystem...");
    status = crypto_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing routing subsystem...");
    status = routing_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing store & forward...");
    status = store_forward_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing netmail...");
    status = netmail_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing message board...");
    status = message_board_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing file transfer...");
    status = file_transfer_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing BBS ANSI...");
    status = bbs_ansi_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing SysOp...");
    status = sysop_init();
    if (status != STATUS_OK) return status;
    
    ESP_LOGI(TAG, "Initializing LoRa mesh...");
    status = lora_mesh_init();
    if (status != STATUS_OK) return status;
    
    return STATUS_OK;
}

// ============================================================================
// ОБРАБОТКА ВХОДЯЩЕГО ПАКЕТА
// ============================================================================
static void handle_incoming_packet(const FullPacket_t* pkt)
{
    const PacketHeader_t* hdr = &pkt->header;
    
    DBG_LOG(TAG_LORA, "Received packet: type=%d src=%d dst=%d", 
            hdr->type, hdr->src, hdr->dst);
    
    if (hdr->dst != MY_NODE_ID && hdr->dst != BROADCAST_ID) {
        routing_process_packet(hdr, hdr->src);
        store_forward_enqueue(pkt, BROADCAST_ID);
        return;
    }
    
    switch (hdr->type) {
        case PKT_CHAT:
            break;
        case PKT_NETMAIL:
            break;
        case PKT_BBS_POST:
            break;
        case PKT_FILE_INFO:
        case PKT_FILE_DATA:
            break;
        case PKT_ROUTE:
        case PKT_ROUTE_REQ:
            break;
        case PKT_NODE:
        case PKT_NODE_REQ:
            break;
        case PKT_ACK:
            store_forward_ack(hdr->src, hdr->seq);
            break;
        case PKT_PING:
            lora_mesh_send_pong(hdr->src);
            break;
        case PKT_REMOTE_CMD:
            sysop_handle_remote_cmd(hdr->src, pkt->payload, hdr->payload_len);
            break;
        case PKT_KEY_EXCH:
            break;
        case PKT_BEACON:
            break;
        default:
            DBG_WARN(TAG_LORA, "Unknown packet type: %d", hdr->type);
            break;
    }
}

// ============================================================================
// ЗАДАЧА BBS
// ============================================================================
static void bbs_task(void* pvParameters)
{
    ESP_LOGI(TAG, "BBS task started");
    
    while (1) {
        bbs_ansi_process();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// ЗАДАЧА ПРИЕМА LoRa
// ============================================================================
static void lora_rx_task(void* pvParameters)
{
    ESP_LOGI(TAG, "LoRa RX task started");
    
    while (1) {
        FullPacket_t pkt;
        if (lora_mesh_receive(&pkt) == STATUS_OK) {
            handle_incoming_packet(&pkt);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// ЗАДАЧА СЕТИ
// ============================================================================
static void network_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Network task started");
    
    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        
        store_forward_process();
        netmail_process_queue();
        file_transfer_cleanup();
        
        if (now - g_last_route_exchange > ROUTE_EXCHANGE_INT) {
            routing_broadcast_table();
            g_last_route_exchange = now;
        }
        
        if (now - g_last_beacon > BEACON_INTERVAL) {
            routing_broadcast_node_info();
            g_last_beacon = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ============================================================================
// ЗАДАЧА ОЧИСТКИ
// ============================================================================
static void cleanup_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Cleanup task started");
    
    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (now - g_last_cleanup > 300000) {
            size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            if (free_heap < HEAP_LOW_WATERMARK) {
                ESP_LOGW(TAG, "Low heap memory: %zu bytes", free_heap);
            }
            
            store_forward_cleanup();
            routing_cleanup();
            netmail_cleanup();
            message_board_cleanup();
            
            g_last_cleanup = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// ============================================================================
// ТОЧКА ВХОДА
// ============================================================================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "LoRa FidoNet BBS Server v%s", NODE_VERSION);
    ESP_LOGI(TAG, "Node ID: %d", MY_NODE_ID);
    ESP_LOGI(TAG, "========================================");
    
    g_startup_time = esp_timer_get_time() / 1000;
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    if (init_filesystem() != STATUS_OK) {
        ESP_LOGE(TAG, "Failed to initialize filesystem");
        return;
    }
    
    if (init_all_subsystems() != STATUS_OK) {
        ESP_LOGE(TAG, "Failed to initialize subsystems");
        return;
    }
    
    g_system_ready = true;
    ESP_LOGI(TAG, "System ready!");
    
    xTaskCreate(bbs_task, "bbs_task", 8192, NULL, 5, NULL);
    xTaskCreate(lora_rx_task, "lora_rx_task", 8192, NULL, 5, NULL);
    xTaskCreate(network_task, "network_task", 8192, NULL, 4, NULL);
    xTaskCreate(cleanup_task, "cleanup_task", 4096, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "All tasks created. System running.");
}