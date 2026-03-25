#pragma once

#include "config.h"
#include "crypto.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ЗАГОЛОВОК ПАКЕТА (16 байт)
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[2];          // 0xAA 0x55 - магические байты
    uint8_t  version;           // Версия протокола
    uint8_t  type;              // Тип пакета (PacketType_t)
    uint16_t src;               // ID отправителя
    uint16_t dst;               // ID получателя
    uint8_t  ttl;               // Time To Live
    uint8_t  hops;              // Количество переходов
    uint16_t seq;               // Номер последовательности
    uint16_t ack;               // Номер подтверждения
    uint8_t  flags;             // Флаги
    uint16_t payload_len;       // Длина полезной нагрузки
    uint8_t  reserved[2];       // Резерв
} PacketHeader_t;
#pragma pack(pop)

// ============================================================================
// ФЛАГИ ПАКЕТА
// ============================================================================
#define PKT_FLAG_ACK_REQ      0x01    // Требуется подтверждение
#define PKT_FLAG_FRAGMENT     0x02    // Фрагментированный пакет
#define PKT_FLAG_LAST_FRAG    0x04    // Последний фрагмент
#define PKT_FLAG_COMPRESSED   0x08    // Сжатые данные
#define PKT_FLAG_ENCRYPTED    0x10    // Зашифрованные данные
#define PKT_FLAG_SIGNED       0x20    // Цифровая подпись
#define PKT_FLAG_BROADCAST    0x40    // Широковещательный
#define PKT_FLAG_PRIORITY     0x80    // Высокий приоритет

// ============================================================================
// МАГИЧЕСКИЕ БАЙТЫ
// ============================================================================
#define PKT_MAGIC_1           0xAA
#define PKT_MAGIC_2           0x55
#define PKT_VERSION           1

// ============================================================================
// ПОЛНЫЙ ПАКЕТ С КРИПТОГРАФИЕЙ
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    PacketHeader_t header;
    uint8_t        payload[MAX_PAYLOAD_SIZE];
    uint8_t        signature[SIGNATURE_SIZE];  // Ed25519 подпись
    uint8_t        aes_tag[AES_TAG_SIZE];      // GCM auth tag
    uint16_t       crc;                        // CRC16 всего пакета
} FullPacket_t;
#pragma pack(pop)

// ============================================================================
// ФРАГМЕНТ ДЛЯ ПЕРЕДАЧИ ФАЙЛОВ
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    uint32_t file_id;
    uint32_t offset;
    uint32_t total_size;
    uint16_t fragment_num;
    uint16_t total_fragments;
    uint8_t  data[MAX_PAYLOAD_SIZE - 14];
} FileFragment_t;
#pragma pack(pop)

// ============================================================================
// ДАННЫЕ ДЛЯ ОБМЕНА КЛЮЧАМИ
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    uint8_t  public_key[KEY_SIZE];    // X25519 публичный ключ
    uint8_t  sign_public[KEY_SIZE];   // Ed25519 публичный ключ
    uint32_t timestamp;
    uint8_t  nonce[12];               // Для защиты от replay
} KeyExchange_t;
#pragma pack(pop)

// ============================================================================
// МАРШРУТНАЯ ИНФОРМАЦИЯ
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    uint16_t dest;
    uint16_t via;
    uint8_t  hops;
    uint8_t  quality;                 // Качество канала (0-255)
    uint32_t last_seen;
} RouteEntry_t;
#pragma pack(pop)

// ============================================================================
// ИНФОРМАЦИЯ ОБ УЗЛЕ
// ============================================================================
#pragma pack(push, 1)
typedef struct {
    uint16_t node_id;
    char     name[32];
    uint8_t  version[8];
    uint8_t  public_key[KEY_SIZE];
    uint8_t  sign_public[KEY_SIZE];
    uint32_t uptime;
    uint8_t  flags;
    uint8_t  reserved[10];
} NodeInfo_t;
#pragma pack(pop)

// ============================================================================
// ФУНКЦИИ РАБОТЫ С ПАКЕТАМИ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация заголовка
void packet_init_header(PacketHeader_t* hdr, PacketType_t type, 
                        uint16_t src, uint16_t dst, uint16_t payload_len);

// Установка флагов
void packet_set_flag(PacketHeader_t* hdr, uint8_t flag);
void packet_clear_flag(PacketHeader_t* hdr, uint8_t flag);
bool packet_has_flag(const PacketHeader_t* hdr, uint8_t flag);

// Валидация пакета
bool packet_validate_magic(const PacketHeader_t* hdr);
bool packet_validate_crc(const FullPacket_t* pkt);

// Расчет CRC16
uint16_t packet_calc_crc(const uint8_t* data, size_t len);

// Сериализация/десериализация
int packet_serialize(const FullPacket_t* pkt, uint8_t* buffer, size_t buf_size);
int packet_deserialize(const uint8_t* buffer, size_t len, FullPacket_t* pkt);

// Клонирование пакета
int packet_clone(const FullPacket_t* src, FullPacket_t* dst);

#ifdef __cplusplus
}
#endif