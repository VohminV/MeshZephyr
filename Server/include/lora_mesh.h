#pragma once

#include "config.h"
#include "packet.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация LoRa модуля
Status_t lora_mesh_init(void);

// Отправка пакета
Status_t lora_mesh_send(const FullPacket_t* pkt);

// Прием пакета
Status_t lora_mesh_receive(FullPacket_t* pkt);

// Отправка PONG ответа
Status_t lora_mesh_send_pong(uint16_t dest);

// Отправка BEACON маяка
Status_t lora_mesh_send_beacon(void);

#ifdef __cplusplus
}
#endif