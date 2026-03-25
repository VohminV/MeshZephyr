#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ИДЕНТИФИКАЦИЯ УЗЛА
// ============================================================================
#define MY_NODE_ID          101             // Уникальный ID клиента (1-254)
#define NODE_NAME           "ROAMER_01"     // Отображаемое имя узла
#define SERVER_NODE_ID      100             // ID главного сервера
#define BROADCAST_ID        0xFFFF          // Широковещательный адрес

// ============================================================================
// ПАРАМЕТРЫ LoRa (ДОЛЖНЫ СОВПАДАТЬ С СЕРВЕРОМ)
// ============================================================================
#define LORA_FREQ           868.0f          // 868 EU, 915 US, 433 Asia
#define LORA_BW             125.0f          // Полоса пропускания kHz
#define LORA_SF             10              // Spreading Factor (7-12)
#define LORA_CR             7               // Coding Rate (5-8)
#define LORA_PREAMBLE       8               // Длина преамбулы
#define LORA_TX_POWER       17              // Мощность dBm (2-17)

// Пины подключения LoRa модуля (SX1276/SX1278)
#define LORA_CS             5               // NSS/CS
#define LORA_RST            14              // Reset
#define LORA_DIO0           2               // DIO0 Interrupt
#define LORA_SCK            18              // SPI Clock
#define LORA_MISO           19              // SPI MISO
#define LORA_MOSI           23              // SPI MOSI

// ============================================================================
// ПАРАМЕТРЫ ПАКЕТОВ
// ============================================================================
#define MAX_PACKET_SIZE     256
#define MAX_PAYLOAD_SIZE    200
#define HEADER_SIZE         16
#define MIN_PACKET_SIZE     16             

#define PKT_MAGIC_1         0xAA
#define PKT_MAGIC_2         0x55
#define PKT_VERSION         1

// Флаги пакетов
#define PKT_FLAG_ACK_REQ    0x01
#define PKT_FLAG_SIGNED     0x20
#define PKT_FLAG_BROADCAST  0x40

// ============================================================================
// КРИПТОГРАФИЯ
// ============================================================================
#define KEY_SIZE            32              // 256 бит
#define SIGNATURE_SIZE      64              // Ed25519
#define AES_KEY_SIZE        16              // AES-128
#define AES_IV_SIZE         12
#define AES_TAG_SIZE        16

// ============================================================================
// ТАЙМИНГИ
// ============================================================================
#define BEACON_INTERVAL     30000           // 30 секунд
#define RETRY_TIMEOUT       5000            // 5 секунд
#define MAX_RETRIES         3
#define ACK_TIMEOUT         3000            // 3 секунды

// ============================================================================
// ЛИМИТЫ
// ============================================================================
#define MAX_KNOWN_NODES     15              // Максимум известных узлов
#define MAX_USERNAME_LEN    32
#define MAX_INPUT_LEN       128

// ============================================================================
// ТИПЫ ПАКЕТОВ (ИСПРАВЛЕНО)
// ============================================================================
typedef enum {
    PKT_UNKNOWN     = 0,
    PKT_PING        = 1,
    PKT_PONG        = 2,
    PKT_BEACON      = 3,        // От клиентов
    PKT_CHAT        = 4,
    PKT_MAIL_REQ    = 5,
    PKT_MAIL_DATA   = 6,
    PKT_FILE_REQ    = 7,
    PKT_FILE_DATA   = 8,
    PKT_BOARD_SYNC  = 9,
    PKT_NODE_LIST   = 10,
    PKT_NODE        = 11,       
    PKT_BBS_SCREEN  = 12,
    PKT_SYSOP       = 13
} PacketType_t;

// ============================================================================
// СТАТУСЫ
// ============================================================================
typedef enum {
    STATUS_OK           = 0,
    STATUS_ERROR        = 1,
    STATUS_TIMEOUT      = 2,
    STATUS_BUSY         = 3,
    STATUS_INVALID      = 4,
    STATUS_NOT_FOUND    = 5,
    STATUS_CRYPTO_FAIL  = 6,
    STATUS_FS_ERROR     = 7
} Status_t;

// ============================================================================
// BBS ЭКРАНЫ
// ============================================================================
typedef enum {
    BBS_SCREEN_LOGIN    = 0,
    BBS_SCREEN_MAIN     = 1,
    BBS_SCREEN_MAIL     = 2,
    BBS_SCREEN_BOARD    = 3,
    BBS_SCREEN_FILES    = 4,
    BBS_SCREEN_CHAT     = 5,
    BBS_SCREEN_NODES    = 6,
    BBS_SCREEN_SYSOP    = 7
} BbsScreen_t;

// ============================================================================
// ОТЛАДКА
// ============================================================================
#define TAG_MAIN        "CLIENT"
#define TAG_LORA        "LORA"
#define TAG_CRYPTO      "CRYPTO"
#define TAG_BBS         "BBS"

#ifdef DEBUG_MODE
    #define DBG_LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
    #define DBG_ERR(fmt, ...) printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#else
    #define DBG_LOG(fmt, ...)
    #define DBG_ERR(fmt, ...)
#endif