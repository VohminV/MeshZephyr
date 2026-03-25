#pragma once

#include "config.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Заголовок пакета (16 байт)
#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[2];          // 0xAA 0x55
    uint8_t  version;           // Версия протокола
    uint8_t  type;              // Тип пакета
    uint16_t src;               // Отправитель
    uint16_t dst;               // Получатель
    uint8_t  ttl;               // Time To Live
    uint8_t  hops;              // Количество переходов
    uint16_t seq;               // Номер последовательности
    uint16_t ack;               // Подтверждение
    uint8_t  flags;             // Флаги
    uint16_t payload_len;       // Длина данных
    uint8_t  reserved[2];       // Резерв
} PacketHeader_t;
#pragma pack(pop)

// Полный пакет
#pragma pack(push, 1)
typedef struct {
    PacketHeader_t header;
    uint8_t        payload[MAX_PAYLOAD_SIZE];
    uint8_t        signature[KEY_SIZE];
    uint8_t        aes_tag[AES_TAG_SIZE];
    uint16_t       crc;
} FullPacket_t;
#pragma pack(pop)

// Функции работы с пакетами
void packet_init_header(PacketHeader_t* hdr, PacketType_t type, 
                        uint16_t src, uint16_t dst, uint16_t payload_len);

void packet_set_flag(PacketHeader_t* hdr, uint8_t flag);

bool packet_has_flag(const PacketHeader_t* hdr, uint8_t flag);

bool packet_validate_magic(const PacketHeader_t* hdr);

uint16_t packet_calc_crc(const uint8_t* data, size_t len);

bool packet_validate_crc(const FullPacket_t* pkt);

int packet_serialize(const FullPacket_t* pkt, uint8_t* buffer, size_t buf_size);

int packet_deserialize(const uint8_t* buffer, size_t len, FullPacket_t* pkt);

#ifdef __cplusplus
}
#endif