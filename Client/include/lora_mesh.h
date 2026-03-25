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

// Проверка доступности пакета
bool lora_mesh_available(void);

// Отправка Ping
Status_t lora_mesh_send_ping(uint16_t dest);

// Отправка Pong
Status_t lora_mesh_send_pong(uint16_t dest);

// Деинициализация
void lora_mesh_deinit(void);

#ifdef __cplusplus
}
#endif