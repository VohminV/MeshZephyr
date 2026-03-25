#pragma once

#include <stdint.h>

// ============================================================================
// ИДЕНТИФИКАЦИЯ УЗЛА
// ============================================================================
#define MY_NODE_ID          100
#define NODE_NAME           "ESP32_BBS_NODE"
#define NODE_VERSION        "1.0.0"
#define BROADCAST_ID        0xFFFF
#define MAX_NODE_ID         0xFFFE

// ============================================================================
// ПАРАМЕТРЫ LoRa
// ============================================================================
#define LORA_FREQ           868.0f
#define LORA_BW             125.0f
#define LORA_SF             10
#define LORA_CR             7
#define LORA_PREAMBLE       8
#define LORA_TX_POWER       17

// Пины для LoRa модуля (настройте под вашу плату)
#define LORA_CS             5
#define LORA_RST            14
#define LORA_DIO0           2
#define LORA_DIO1           4  // Для SX126x
#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23

// ============================================================================
// ПАРАМЕТРЫ ПАКЕТОВ
// ============================================================================
#define MAX_PACKET_SIZE     256
#define MAX_PAYLOAD_SIZE    200
#define HEADER_SIZE         16
#define MIN_PACKET_SIZE     (HEADER_SIZE + 4)  // Header + CRC

#define MAIL_TTL            10
#define CHAT_TTL            5
#define FILE_TTL            15
#define ROUTE_TTL           20

#define MAX_HOPS            15
#define DEFAULT_HOPS        0

// ============================================================================
// ТАЙМИНГИ
// ============================================================================
#define ROUTE_TIMEOUT       3600000UL     // 1 час
#define STORE_TIMEOUT       86400000UL    // 24 часа
#define ROUTE_EXCHANGE_INT  300000UL      // 5 минут
#define BEACON_INTERVAL     60000UL       // 1 минута
#define RETRY_TIMEOUT       5000UL        // 5 секунд
#define MAX_RETRIES         3
#define ACK_TIMEOUT         3000UL        // 3 секунды

// ============================================================================
// ЛИМИТЫ ХРАНЕНИЯ
// ============================================================================
#define MAX_STORED_PACKETS  50
#define MAX_MAILBOX_SIZE    100
#define MAX_BOARD_MESSAGES  200
#define MAX_FILE_QUEUE      20
#define MAX_ROUTE_ENTRIES   30
#define MAX_NODE_ENTRIES    20

// ============================================================================
// КРИПТОГРАФИЯ
// ============================================================================
#define KEY_SIZE            32              // 256 бит для X25519/Ed25519
#define SIGNATURE_SIZE      64              // Ed25519 подпись
#define AES_KEY_SIZE        16              // AES-128
#define AES_IV_SIZE         12              // GCM nonce
#define AES_TAG_SIZE        16              // GCM auth tag
#define SESSION_KEY_EXPIRY  3600000UL       // 1 час

// ============================================================================
// СЖАТИЕ
// ============================================================================
#define COMPRESSION_THRESHOLD 50            // Минимальный размер для сжатия
#define MAX_COMPRESS_RATIO  4

// ============================================================================
// ФАЙЛОВАЯ СИСТЕМА
// ============================================================================
#define FS_MOUNT_POINT      "/bbs"
#define MAIL_IN_DIR         "/bbs/mail/in"
#define MAIL_OUT_DIR        "/bbs/mail/out"
#define MAIL_SENT_DIR       "/bbs/mail/sent"
#define BOARD_DIR           "/bbs/board"
#define FILES_DIR           "/bbs/files"
#define CONFIG_DIR          "/bbs/config"
#define KEYS_DIR            "/bbs/keys"
#define LOGS_DIR            "/bbs/logs"

// ============================================================================
// ПОЛЬЗОВАТЕЛИ
// ============================================================================
#define MAX_USERS           50
#define MAX_USERNAME_LEN    32
#define MAX_PASSWORD_LEN    64
#define MAX_SESSION_TIME    1800000UL       // 30 минут

// ============================================================================
// СИСТЕМНЫЕ КОНСТАНТЫ
// ============================================================================
#define SERIAL_BAUD         115200
#define WATCHDOG_TIMEOUT    60000           // 60 секунд
#define HEAP_LOW_WATERMARK  50000           // 50KB

// ============================================================================
// ТИПЫ ПАКЕТОВ
// ============================================================================
typedef enum {
    PKT_UNKNOWN     = 0,
    PKT_CHAT        = 1,
    PKT_NETMAIL     = 2,
    PKT_BBS_POST    = 3,
    PKT_FILE_INFO   = 4,
    PKT_FILE_DATA   = 5,
    PKT_FILE_ACK    = 6,
    PKT_ROUTE       = 7,
    PKT_ROUTE_REQ   = 8,
    PKT_NODE        = 9,
    PKT_NODE_REQ    = 10,
    PKT_ACK         = 11,
    PKT_NACK        = 12,
    PKT_PING        = 13,
    PKT_PONG        = 14,
    PKT_REMOTE_CMD  = 15,
    PKT_KEY_EXCH    = 16,
    PKT_BEACON      = 17,
    PKT_SYSOP       = 18
} PacketType_t;

// ============================================================================
// СТАТУСЫ
// ============================================================================
typedef enum {
    STATUS_OK           = 0,
    STATUS_ERROR        = 1,
    STATUS_BUSY         = 2,
    STATUS_TIMEOUT      = 3,
    STATUS_NOT_FOUND    = 4,
    STATUS_INVALID      = 5,
    STATUS_NO_ROUTE     = 6,
    STATUS_CRYPTO_FAIL  = 7,
    STATUS_QUEUE_FULL   = 8,
    STATUS_FS_ERROR     = 9
} Status_t;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МАКРОСЫ
// ============================================================================
#define ARRAY_SIZE(x)       (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)    (MIN(MAX(x, lo), hi))

// ============================================================================
// ОТЛАДКА
// ============================================================================
#ifdef DEBUG_MODE
    #define DBG_LOG(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
    #define DBG_ERR(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
    #define DBG_WARN(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
    #define DBG_DUMP(tag, data, len) ESP_LOG_BUFFER_HEXDUMP(tag, data, len, ESP_LOG_INFO)
#else
    #define DBG_LOG(tag, format, ...)
    #define DBG_ERR(tag, format, ...)
    #define DBG_WARN(tag, format, ...)
    #define DBG_DUMP(tag, data, len)
#endif

#define TAG_MAIN        "BBS_MAIN"
#define TAG_LORA        "BBS_LORA"
#define TAG_CRYPTO      "BBS_CRYPTO"
#define TAG_ROUTING     "BBS_ROUTE"
#define TAG_MAIL        "BBS_MAIL"
#define TAG_BOARD       "BBS_BOARD"
#define TAG_FILE        "BBS_FILE"
#define TAG_BBS         "BBS_ANSI"
#define TAG_SYSOP       "BBS_SYSOP"