#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"

#include "config.h"
#include "message_board.h"
#include "packet.h"
#include "lora_mesh.h"

static const char* TAG = TAG_BOARD;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static BoardMessage_t g_messages[MAX_BOARD_MESSAGES];
static uint16_t g_message_count = 0;
static uint32_t g_next_message_id = 1;
static bool g_initialized = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ДОСКИ ОБЪЯВЛЕНИЙ
// ============================================================================
Status_t message_board_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing Message Board subsystem...");
    
    memset(g_messages, 0, sizeof(g_messages));
    g_message_count = 0;
    g_next_message_id = 1;
    
    mkdir(BOARD_DIR, 0755);
    
    g_initialized = true;
    ESP_LOGI(TAG, "Message Board initialized");
    
    return STATUS_OK;
}

// ============================================================================
// ПУБЛИКАЦИЯ СООБЩЕНИЯ
// ============================================================================
Status_t message_board_post(uint16_t author, const char* author_name,
                            const char* subject, const char* text)
{
    if (!subject || !text) {
        return STATUS_INVALID;
    }
    
    if (g_message_count >= MAX_BOARD_MESSAGES) {
        message_board_cleanup();
    }
    
    BoardMessage_t* msg = &g_messages[g_message_count];
    memset(msg, 0, sizeof(BoardMessage_t));
    
    msg->id = g_next_message_id++;
    msg->author = author;
    strncpy(msg->author_name, author_name ? author_name : "Anonymous", 
            sizeof(msg->author_name) - 1);
    strncpy(msg->subject, subject, sizeof(msg->subject) - 1);
    msg->timestamp = esp_timer_get_time() / 1000;
    msg->size = strlen(text);
    msg->flags = 0;
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.msg", BOARD_DIR, (unsigned long)msg->id);
    
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "ID:%lu\n", (unsigned long)msg->id);
        fprintf(f, "AUTHOR:%d\n", msg->author);
        fprintf(f, "AUTHOR_NAME:%s\n", msg->author_name);
        fprintf(f, "SUBJECT:%s\n", msg->subject);
        fprintf(f, "TIMESTAMP:%lu\n", (unsigned long)msg->timestamp);
        fprintf(f, "SIZE:%lu\n", (unsigned long)msg->size);
        fprintf(f, "\n---TEXT---\n");
        fprintf(f, "%s", text);
        fclose(f);
    }
    
    g_message_count++;
    
    ESP_LOGI(TAG, "Message posted: id=%lu subject=%s", (unsigned long)msg->id, msg->subject);
    
    return STATUS_OK;
}

// ============================================================================
// ЧТЕНИЕ СООБЩЕНИЯ
// ============================================================================
Status_t message_board_read(uint32_t id, BoardMessage_t* msg, 
                            char* text, size_t text_size)
{
    if (!msg) {
        return STATUS_INVALID;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.msg", BOARD_DIR, (unsigned long)id);
    
    FILE* f = fopen(path, "r");
    if (!f) {
        return STATUS_NOT_FOUND;
    }
    
    char line[256];
    bool in_text = false;
    size_t text_offset = 0;
    
    memset(msg, 0, sizeof(BoardMessage_t));
    if (text) text[0] = '\0';
    
    while (fgets(line, sizeof(line), f)) {
        if (in_text) {
            if (text && text_offset < text_size - 1) {
                text_offset += snprintf(text + text_offset, 
                                        text_size - text_offset, "%s", line);
            }
        } else if (strcmp(line, "\n---TEXT---\n") == 0) {
            in_text = true;
        } else {
            char* colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char* value = colon + 1;
                while (*value == ' ') value++;
                char* newline = strchr(value, '\n');
                if (newline) *newline = '\0';
                
                if (strcmp(line, "ID") == 0) msg->id = atol(value);
                else if (strcmp(line, "AUTHOR") == 0) msg->author = atoi(value);
                else if (strcmp(line, "AUTHOR_NAME") == 0) strncpy(msg->author_name, value, sizeof(msg->author_name)-1);
                else if (strcmp(line, "SUBJECT") == 0) strncpy(msg->subject, value, sizeof(msg->subject)-1);
                else if (strcmp(line, "TIMESTAMP") == 0) msg->timestamp = atol(value);
                else if (strcmp(line, "SIZE") == 0) msg->size = atol(value);
            }
        }
    }
    
    fclose(f);
    
    return STATUS_OK;
}

// ============================================================================
// ПОЛУЧЕНИЕ КОЛИЧЕСТВА СООБЩЕНИЙ
// ============================================================================
uint16_t message_board_get_count(void)
{
    return g_message_count;
}

// ============================================================================
// СПИСОК СООБЩЕНИЙ
// ============================================================================
Status_t message_board_list(BoardMessage_t* list, uint16_t start, uint16_t count)
{
    if (!list) {
        return STATUS_INVALID;
    }
    
    uint16_t idx = 0;
    uint16_t copied = 0;
    
    for (idx = start; idx < g_message_count && copied < count; idx++) {
        list[copied] = g_messages[idx];
        copied++;
    }
    
    return STATUS_OK;
}

// ============================================================================
// УДАЛЕНИЕ СООБЩЕНИЯ
// ============================================================================
Status_t message_board_delete(uint32_t id)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.msg", BOARD_DIR, (unsigned long)id);
    
    if (remove(path) == 0) {
        for (uint16_t i = 0; i < g_message_count; i++) {
            if (g_messages[i].id == id) {
                for (uint16_t j = i; j < g_message_count - 1; j++) {
                    g_messages[j] = g_messages[j + 1];
                }
                g_message_count--;
                break;
            }
        }
        
        ESP_LOGI(TAG, "Message deleted: %lu", (unsigned long)id);
        return STATUS_OK;
    }
    
    return STATUS_NOT_FOUND;
}

// ============================================================================
// ОЧИСТКА СТАРЫХ СООБЩЕНИЙ
// ============================================================================
void message_board_cleanup(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    uint32_t max_age = 90 * 24 * 3600;
    
    DIR* dir = opendir(BOARD_DIR);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".msg")) {
                char path[128];
                snprintf(path, sizeof(path), "%s/%s", BOARD_DIR, entry->d_name);
                
                FILE* f = fopen(path, "r");
                if (f) {
                    char line[256];
                    while (fgets(line, sizeof(line), f)) {
                        if (strncmp(line, "TIMESTAMP:", 10) == 0) {
                            uint32_t timestamp = atol(line + 10);
                            if (now - timestamp > max_age) {
                                fclose(f);
                                remove(path);
                                ESP_LOGI(TAG, "Old message cleaned: %s", entry->d_name);
                            }
                            break;
                        }
                    }
                    fclose(f);
                }
            }
        }
        closedir(dir);
    }
    
    g_message_count = 0;
    DIR* dir2 = opendir(BOARD_DIR);
    if (dir2) {
        struct dirent* entry;
        while ((entry = readdir(dir2)) != NULL) {
            if (strstr(entry->d_name, ".msg")) {
                g_message_count++;
            }
        }
        closedir(dir2);
    }
}

// ============================================================================
// РАССЫЛКА СООБЩЕНИЯ ПО СЕТИ
// ============================================================================
Status_t message_board_broadcast(const BoardMessage_t* msg, const char* text)
{
    if (!msg || !text) {
        return STATUS_INVALID;
    }
    
    FullPacket_t pkt;
    size_t payload_len = sizeof(BoardMessage_t) + strlen(text) + 1;
    
    if (payload_len > MAX_PAYLOAD_SIZE) {
        return STATUS_INVALID;
    }
    
    packet_init_header(&pkt.header, PKT_BBS_POST, MY_NODE_ID, BROADCAST_ID, payload_len);
    pkt.header.flags = PKT_FLAG_SIGNED | PKT_FLAG_BROADCAST;
    pkt.header.ttl = 10;
    
    memcpy(pkt.payload, msg, sizeof(BoardMessage_t));
    memcpy(pkt.payload + sizeof(BoardMessage_t), text, strlen(text) + 1);
    
    Status_t status = lora_mesh_send(&pkt);
    ESP_LOGI(TAG, "Message broadcast: %s", status == STATUS_OK ? "OK" : "FAIL");
    
    return status;
}