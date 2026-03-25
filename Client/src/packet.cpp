#include <stdio.h>
#include <string.h>
#include "packet.h"

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
    hdr->ttl = 10;
    hdr->hops = 0;
    hdr->seq = 0;
    hdr->ack = 0;
    hdr->flags = 0;
    hdr->payload_len = payload_len;
}

void packet_set_flag(PacketHeader_t* hdr, uint8_t flag)
{
    if (hdr) hdr->flags |= flag;
}

bool packet_has_flag(const PacketHeader_t* hdr, uint8_t flag)
{
    return (hdr && (hdr->flags & flag)) ? true : false;
}

bool packet_validate_magic(const PacketHeader_t* hdr)
{
    if (!hdr) return false;
    return (hdr->magic[0] == PKT_MAGIC_1 && hdr->magic[1] == PKT_MAGIC_2);
}

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

bool packet_validate_crc(const FullPacket_t* pkt)
{
    if (!pkt) return false;
    uint16_t calc_crc = packet_calc_crc((uint8_t*)&pkt->header, 
                                         sizeof(PacketHeader_t) + pkt->header.payload_len);
    return (pkt->crc == calc_crc);
}

int packet_serialize(const FullPacket_t* pkt, uint8_t* buffer, size_t buf_size)
{
    if (!pkt || !buffer) return -1;
    
    size_t required = sizeof(PacketHeader_t) + pkt->header.payload_len + 2;
    if (required > buf_size) return -1;
    
    size_t offset = 0;
    
    memcpy(buffer + offset, &pkt->header, sizeof(PacketHeader_t));
    offset += sizeof(PacketHeader_t);
    
    if (pkt->header.payload_len > 0) {
        memcpy(buffer + offset, pkt->payload, pkt->header.payload_len);
        offset += pkt->header.payload_len;
    }
    
    uint16_t crc = packet_calc_crc(buffer, offset);
    buffer[offset++] = crc & 0xFF;
    buffer[offset++] = (crc >> 8) & 0xFF;
    
    return (int)offset;
}

int packet_deserialize(const uint8_t* buffer, size_t len, FullPacket_t* pkt)
{
    if (!buffer || !pkt || len < sizeof(PacketHeader_t) + 2) return -1;
    
    memset(pkt, 0, sizeof(FullPacket_t));
    
    size_t offset = 0;
    
    memcpy(&pkt->header, buffer + offset, sizeof(PacketHeader_t));
    offset += sizeof(PacketHeader_t);
    
    if (!packet_validate_magic(&pkt->header)) return -1;
    
    if (len < sizeof(PacketHeader_t) + pkt->header.payload_len + 2) return -1;
    
    if (pkt->header.payload_len > 0) {
        memcpy(pkt->payload, buffer + offset, pkt->header.payload_len);
        offset += pkt->header.payload_len;
    }
    
    uint16_t stored_crc = buffer[offset] | (buffer[offset + 1] << 8);
    uint16_t calc_crc = packet_calc_crc(buffer, len - 2);
    
    if (stored_crc != calc_crc) return -1;
    
    pkt->crc = stored_crc;
    
    return (int)len;
}