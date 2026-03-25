#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "config.h"
#include "packet.h"

static const char* TAG = "BBS_PACKET";

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ЗАГОЛОВКА
// ============================================================================
void packet_init_header(PacketHeader_t* hdr, PacketType_t type, 
                        uint16_t src, uint16_t dst, uint16_t payload_len)
{
    if (!hdr) return;
    
    memset(hdr, 0, sizeof(PacketHeader_t));
    
    hdr->magic[0] = PKT_MAGIC_1;
    hdr->magic[1] = PKT_MAGIC_2;
    hdr->version = PKT_VERSION;
    hdr->type = (uint8_t)type;
    hdr->src = src;
    hdr->dst = dst;
    hdr->ttl = (type == PKT_NETMAIL) ? MAIL_TTL :
               (type == PKT_CHAT) ? CHAT_TTL :
               (type == PKT_FILE_DATA) ? FILE_TTL : ROUTE_TTL;
    hdr->hops = DEFAULT_HOPS;
    hdr->seq = 0;  // Устанавливается отправителем
    hdr->ack = 0;
    hdr->flags = 0;
    hdr->payload_len = payload_len;
    hdr->reserved[0] = 0;
    hdr->reserved[1] = 0;
}

// ============================================================================
// УСТАНОВКА ФЛАГОВ
// ============================================================================
void packet_set_flag(PacketHeader_t* hdr, uint8_t flag)
{
    if (hdr) {
        hdr->flags |= flag;
    }
}

void packet_clear_flag(PacketHeader_t* hdr, uint8_t flag)
{
    if (hdr) {
        hdr->flags &= ~flag;
    }
}

bool packet_has_flag(const PacketHeader_t* hdr, uint8_t flag)
{
    return (hdr && (hdr->flags & flag)) ? true : false;
}

// ============================================================================
// ВАЛИДАЦИЯ МАГИЧЕСКИХ БАЙТОВ
// ============================================================================
bool packet_validate_magic(const PacketHeader_t* hdr)
{
    if (!hdr) return false;
    return (hdr->magic[0] == PKT_MAGIC_1 && hdr->magic[1] == PKT_MAGIC_2);
}

// ============================================================================
// РАСЧЕТ CRC16
// ============================================================================
uint16_t packet_calc_crc(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}

// ============================================================================
// ВАЛИДАЦИЯ CRC
// ============================================================================
bool packet_validate_crc(const FullPacket_t* pkt)
{
    if (!pkt) return false;
    
    uint16_t stored_crc = pkt->crc;
    
    // Временное обнуление CRC для расчета
    uint16_t calc_crc = packet_calc_crc((uint8_t*)&pkt->header, 
                                         sizeof(PacketHeader_t) + 
                                         pkt->header.payload_len);
    
    return (stored_crc == calc_crc);
}

// ============================================================================
// СЕРИАЛИЗАЦИЯ ПАКЕТА
// ============================================================================
int packet_serialize(const FullPacket_t* pkt, uint8_t* buffer, size_t buf_size)
{
    if (!pkt || !buffer) return -1;
    
    size_t required = sizeof(PacketHeader_t) + pkt->header.payload_len;
    
    if (pkt->header.flags & PKT_FLAG_SIGNED) {
        required += SIGNATURE_SIZE;
    }
    
    if (pkt->header.flags & PKT_FLAG_ENCRYPTED) {
        required += AES_TAG_SIZE;
    }
    
    required += 2;  // CRC
    
    if (required > buf_size) {
        return -1;
    }
    
    size_t offset = 0;
    
    // Копирование заголовка
    memcpy(buffer + offset, &pkt->header, sizeof(PacketHeader_t));
    offset += sizeof(PacketHeader_t);
    
    // Копирование полезной нагрузки
    if (pkt->header.payload_len > 0) {
        memcpy(buffer + offset, pkt->payload, pkt->header.payload_len);
        offset += pkt->header.payload_len;
    }
    
    // Копирование подписи
    if (pkt->header.flags & PKT_FLAG_SIGNED) {
        memcpy(buffer + offset, pkt->signature, SIGNATURE_SIZE);
        offset += SIGNATURE_SIZE;
    }
    
    // Копирование AES тега
    if (pkt->header.flags & PKT_FLAG_ENCRYPTED) {
        memcpy(buffer + offset, pkt->aes_tag, AES_TAG_SIZE);
        offset += AES_TAG_SIZE;
    }
    
    // Расчет и запись CRC
    uint16_t crc = packet_calc_crc(buffer, offset);
    buffer[offset++] = crc & 0xFF;
    buffer[offset++] = (crc >> 8) & 0xFF;
    
    return (int)offset;
}

// ============================================================================
// ДЕСЕРИАЛИЗАЦИЯ ПАКЕТА
// ============================================================================
int packet_deserialize(const uint8_t* buffer, size_t len, FullPacket_t* pkt)
{
    if (!buffer || !pkt || len < sizeof(PacketHeader_t) + 2) {
        return -1;
    }
    
    memset(pkt, 0, sizeof(FullPacket_t));
    
    size_t offset = 0;
    
    // Чтение заголовка
    memcpy(&pkt->header, buffer + offset, sizeof(PacketHeader_t));
    offset += sizeof(PacketHeader_t);
    
    // Валидация магических байтов
    if (!packet_validate_magic(&pkt->header)) {
        return -1;
    }
    
    // Проверка длины
    if (len < sizeof(PacketHeader_t) + pkt->header.payload_len + 2) {
        return -1;
    }
    
    // Чтение полезной нагрузки
    if (pkt->header.payload_len > 0) {
        memcpy(pkt->payload, buffer + offset, pkt->header.payload_len);
        offset += pkt->header.payload_len;
    }
    
    // Чтение подписи
    if (pkt->header.flags & PKT_FLAG_SIGNED) {
        if (offset + SIGNATURE_SIZE > len - 2) {
            return -1;
        }
        memcpy(pkt->signature, buffer + offset, SIGNATURE_SIZE);
        offset += SIGNATURE_SIZE;
    }
    
    // Чтение AES тега
    if (pkt->header.flags & PKT_FLAG_ENCRYPTED) {
        if (offset + AES_TAG_SIZE > len - 2) {
            return -1;
        }
        memcpy(pkt->aes_tag, buffer + offset, AES_TAG_SIZE);
        offset += AES_TAG_SIZE;
    }
    
    // Чтение и проверка CRC
    uint16_t stored_crc = buffer[offset] | (buffer[offset + 1] << 8);
    uint16_t calc_crc = packet_calc_crc(buffer, len - 2);
    
    if (stored_crc != calc_crc) {
        return -1;
    }
    
    pkt->crc = stored_crc;
    
    return (int)len;
}

// ============================================================================
// КЛОНИРОВАНИЕ ПАКЕТА
// ============================================================================
int packet_clone(const FullPacket_t* src, FullPacket_t* dst)
{
    if (!src || !dst) {
        return -1;
    }
    
    memcpy(dst, src, sizeof(FullPacket_t));
    
    return 0;
}