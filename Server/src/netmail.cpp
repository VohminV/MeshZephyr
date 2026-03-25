#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"

#include "config.h"
#include "netmail.h"
#include "packet.h"
#include "lora_mesh.h"
#include "routing.h"

static const char* TAG = TAG_MAIL;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static NetMail_t g_mailbox[MAX_MAILBOX_SIZE];
static uint8_t g_mail_count = 0;
static uint32_t g_next_mail_id = 1;
static bool g_initialized = false;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ПОЧТЫ
// ============================================================================
Status_t netmail_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing NetMail subsystem...");
    
    memset(g_mailbox, 0, sizeof(g_mailbox));
    g_mail_count = 0;
    g_next_mail_id = 1;
    
    netmail_cleanup();
    
    g_initialized = true;
    ESP_LOGI(TAG, "NetMail subsystem initialized");
    
    return STATUS_OK;
}

// ============================================================================
// СОЗДАНИЕ ПИСЬМА
// ============================================================================
Status_t netmail_create(NetMail_t* mail, uint16_t to, const char* subject)
{
    if (!mail || !subject) {
        return STATUS_INVALID;
    }
    
    memset(mail, 0, sizeof(NetMail_t));
    
    mail->id = g_next_mail_id++;
    mail->from = MY_NODE_ID;
    mail->to = to;
    strncpy(mail->subject, subject, sizeof(mail->subject) - 1);
    mail->timestamp = esp_timer_get_time() / 1000;
    mail->status = 0;
    mail->flags = 0;
    
    const NodeInfo_t* node = routing_find_node(MY_NODE_ID);
    if (node) {
        strncpy(mail->from_name, node->name, sizeof(mail->from_name) - 1);
    }
    
    return STATUS_OK;
}

// ============================================================================
// СОХРАНЕНИЕ ПИСЬМА
// ============================================================================
Status_t netmail_save(const NetMail_t* mail, const char* body)
{
    if (!mail) {
        return STATUS_INVALID;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.msg", 
             (mail->to == MY_NODE_ID) ? MAIL_IN_DIR : MAIL_OUT_DIR,
             (unsigned long)mail->id);
    
    FILE* f = fopen(path, "w");
    if (!f) {
        return STATUS_FS_ERROR;
    }
    
    fprintf(f, "ID:%lu\n", (unsigned long)mail->id);
    fprintf(f, "FROM:%d\n", mail->from);
    fprintf(f, "TO:%d\n", mail->to);
    fprintf(f, "FROM_NAME:%s\n", mail->from_name);
    fprintf(f, "TO_NAME:%s\n", mail->to_name);
    fprintf(f, "SUBJECT:%s\n", mail->subject);
    fprintf(f, "TIMESTAMP:%lu\n", (unsigned long)mail->timestamp);
    fprintf(f, "FLAGS:%d\n", mail->flags);
    fprintf(f, "STATUS:%d\n", mail->status);
    fprintf(f, "SIZE:%lu\n", (unsigned long)mail->size);
    fprintf(f, "\n---BODY---\n");
    if (body) {
        fprintf(f, "%s", body);
    }
    
    fclose(f);
    
    DBG_LOG(TAG, "Mail saved: %s", path);
    
    return STATUS_OK;
}

// ============================================================================
// ЗАГРУЗКА ПИСЬМА
// ============================================================================
Status_t netmail_load(uint32_t id, NetMail_t* mail, char* body, size_t body_size)
{
    if (!mail) {
        return STATUS_INVALID;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.msg", MAIL_IN_DIR, (unsigned long)id);
    
    FILE* f = fopen(path, "r");
    if (!f) {
        snprintf(path, sizeof(path), "%s/%lu.msg", MAIL_OUT_DIR, (unsigned long)id);
        f = fopen(path, "r");
    }
    
    if (!f) {
        return STATUS_NOT_FOUND;
    }
    
    char line[256];
    bool in_body = false;
    size_t body_offset = 0;
    
    memset(mail, 0, sizeof(NetMail_t));
    if (body) body[0] = '\0';
    
    while (fgets(line, sizeof(line), f)) {
        if (in_body) {
            if (body && body_offset < body_size - 1) {
                body_offset += snprintf(body + body_offset, 
                                        body_size - body_offset, "%s", line);
            }
        } else if (strcmp(line, "\n---BODY---\n") == 0) {
            in_body = true;
        } else {
            char* colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char* value = colon + 1;
                while (*value == ' ') value++;
                char* newline = strchr(value, '\n');
                if (newline) *newline = '\0';
                
                if (strcmp(line, "ID") == 0) mail->id = atol(value);
                else if (strcmp(line, "FROM") == 0) mail->from = atoi(value);
                else if (strcmp(line, "TO") == 0) mail->to = atoi(value);
                else if (strcmp(line, "FROM_NAME") == 0) strncpy(mail->from_name, value, sizeof(mail->from_name)-1);
                else if (strcmp(line, "TO_NAME") == 0) strncpy(mail->to_name, value, sizeof(mail->to_name)-1);
                else if (strcmp(line, "SUBJECT") == 0) strncpy(mail->subject, value, sizeof(mail->subject)-1);
                else if (strcmp(line, "TIMESTAMP") == 0) mail->timestamp = atol(value);
                else if (strcmp(line, "FLAGS") == 0) mail->flags = atoi(value);
                else if (strcmp(line, "STATUS") == 0) mail->status = atoi(value);
                else if (strcmp(line, "SIZE") == 0) mail->size = atol(value);
            }
        }
    }
    
    fclose(f);
    
    return STATUS_OK;
}

// ============================================================================
// ДОБАВЛЕНИЕ ВХОДЯЩЕГО ПИСЬМА
// ============================================================================
Status_t netmail_add_incoming(const NetMail_t* mail, const char* body)
{
    if (!mail) {
        return STATUS_INVALID;
    }
    
    netmail_save(mail, body);
    
    if (g_mail_count < MAX_MAILBOX_SIZE) {
        g_mailbox[g_mail_count] = *mail;
        g_mail_count++;
    }
    
    DBG_LOG(TAG, "Incoming mail added: from=%d subject=%s", mail->from, mail->subject);
    
    return STATUS_OK;
}

// ============================================================================
// ПОСТАНОВКА В ОЧЕРЕДЬ ОТПРАВКИ
// ============================================================================
Status_t netmail_queue_outgoing(uint32_t mail_id)
{
    NetMail_t mail;
    char body[1024];
    
    if (netmail_load(mail_id, &mail, body, sizeof(body)) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }
    
    uint16_t via;
    uint8_t hops;
    if (routing_find_route(mail.to, &via, &hops) != STATUS_OK) {
        mail.status = 3;
        netmail_save(&mail, body);
        return STATUS_NO_ROUTE;
    }
    
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_NETMAIL, MY_NODE_ID, mail.to, 
                       sizeof(NetMail_t) + strlen(body) + 1);
    pkt.header.flags = PKT_FLAG_SIGNED | PKT_FLAG_ENCRYPTED | PKT_FLAG_ACK_REQ;
    
    memcpy(pkt.payload, &mail, sizeof(NetMail_t));
    memcpy(pkt.payload + sizeof(NetMail_t), body, strlen(body) + 1);
    
    Status_t status = lora_mesh_send(&pkt);
    
    if (status == STATUS_OK) {
        mail.status = 2;
        netmail_save(&mail, body);
        DBG_LOG(TAG, "Mail queued for sending: %lu", (unsigned long)mail_id);
    } else {
        mail.status = 3;
        netmail_save(&mail, body);
    }
    
    return status;
}

// ============================================================================
// ОБРАБОТКА ОЧЕРЕДИ ОТПРАВКИ
// ============================================================================
Status_t netmail_process_queue(void)
{
    DIR* dir = opendir(MAIL_OUT_DIR);
    if (!dir) {
        return STATUS_FS_ERROR;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".msg")) {
            char path[128];
            snprintf(path, sizeof(path), "%s/%s", MAIL_OUT_DIR, entry->d_name);
            
            FILE* f = fopen(path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "STATUS:", 7) == 0) {
                        int status = atoi(line + 7);
                        if (status == 0) {
                            fclose(f);
                            char* dot = strchr(entry->d_name, '.');
                            if (dot) {
                                *dot = '\0';
                                uint32_t mail_id = atoi(entry->d_name);
                                netmail_queue_outgoing(mail_id);
                            }
                            break;
                        }
                        break;
                    }
                }
                fclose(f);
            }
        }
    }
    
    closedir(dir);
    
    return STATUS_OK;
}

// ============================================================================
// УДАЛЕНИЕ ПИСЬМА
// ============================================================================
Status_t netmail_delete(uint32_t id)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%lu.msg", MAIL_IN_DIR, (unsigned long)id);
    
    if (remove(path) == 0) {
        DBG_LOG(TAG, "Mail deleted: %lu", (unsigned long)id);
        return STATUS_OK;
    }
    
    snprintf(path, sizeof(path), "%s/%lu.msg", MAIL_OUT_DIR, (unsigned long)id);
    if (remove(path) == 0) {
        DBG_LOG(TAG, "Mail deleted: %lu", (unsigned long)id);
        return STATUS_OK;
    }
    
    return STATUS_NOT_FOUND;
}

// ============================================================================
// ПОЛУЧЕНИЕ КОЛИЧЕСТВА ПИСЕМ
// ============================================================================
uint16_t netmail_get_inbox_count(void)
{
    uint16_t count = 0;
    
    DIR* dir = opendir(MAIL_IN_DIR);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".msg")) {
                count++;
            }
        }
        closedir(dir);
    }
    
    return count;
}

uint16_t netmail_get_outbox_count(void)
{
    uint16_t count = 0;
    
    DIR* dir = opendir(MAIL_OUT_DIR);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".msg")) {
                count++;
            }
        }
        closedir(dir);
    }
    
    return count;
}

// ============================================================================
// СПИСОК ПИСЕМ
// ============================================================================
Status_t netmail_list_inbox(NetMail_t* list, uint16_t max_count)
{
    if (!list) {
        return STATUS_INVALID;
    }
    
    uint16_t count = 0;
    
    DIR* dir = opendir(MAIL_IN_DIR);
    if (!dir) {
        return STATUS_FS_ERROR;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (strstr(entry->d_name, ".msg")) {
            char* dot = strchr(entry->d_name, '.');
            if (dot) {
                *dot = '\0';
                uint32_t mail_id = atoi(entry->d_name);
                netmail_load(mail_id, &list[count], NULL, 0);
                count++;
            }
        }
    }
    
    closedir(dir);
    
    return STATUS_OK;
}

// ============================================================================
// ПОИСК ПИСЬМА
// ============================================================================
NetMail_t* netmail_find(uint32_t id)
{
    for (uint8_t i = 0; i < g_mail_count; i++) {
        if (g_mailbox[i].id == id) {
            return &g_mailbox[i];
        }
    }
    
    return NULL;
}

// ============================================================================
// ОЧИСТКА СТАРЫХ ПИСЕМ
// ============================================================================
void netmail_cleanup(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    uint32_t max_age = 30 * 24 * 3600;
    
    DIR* dir = opendir(MAIL_IN_DIR);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".msg")) {
                char path[128];
                snprintf(path, sizeof(path), "%s/%s", MAIL_IN_DIR, entry->d_name);
                
                FILE* f = fopen(path, "r");
                if (f) {
                    char line[256];
                    while (fgets(line, sizeof(line), f)) {
                        if (strncmp(line, "TIMESTAMP:", 10) == 0) {
                            uint32_t timestamp = atol(line + 10);
                            if (now - timestamp > max_age) {
                                fclose(f);
                                remove(path);
                                DBG_LOG(TAG, "Old mail cleaned: %s", entry->d_name);
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
}