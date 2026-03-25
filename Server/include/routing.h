#pragma once

#include "config.h"
#include "packet.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// ТАБЛИЦА МАРШРУТИЗАЦИИ
// ============================================================================
typedef struct {
    RouteEntry_t entries[MAX_ROUTE_ENTRIES];
    uint8_t      count;
    uint32_t     last_update;
} RoutingTable_t;

// ============================================================================
// ОЧЕРЕДЬ МАРШРУТИЗАЦИИ
// ============================================================================
typedef struct {
    uint16_t     dest;
    uint16_t     via;
    uint8_t      hops;
    uint32_t     timestamp;
    uint8_t      retries;
} RouteRequest_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация подсистемы маршрутизации
Status_t routing_init(void);

// Обработка входящего пакета
Status_t routing_process_packet(const PacketHeader_t* hdr, uint16_t next_hop);

// Поиск маршрута
Status_t routing_find_route(uint16_t dest, uint16_t* via, uint8_t* hops);

// Добавление/обновление маршрута
Status_t routing_add_route(uint16_t dest, uint16_t via, uint8_t hops, uint8_t quality);

// Удаление устаревших маршрутов
void routing_cleanup(void);

// Получение таблицы маршрутов
const RoutingTable_t* routing_get_table(void);

// ============================================================================
// ОБМЕН МАРШРУТАМИ
// ============================================================================

// Рассылка таблицы маршрутов соседям
Status_t routing_broadcast_table(void);

// Запрос маршрута к узлу
Status_t routing_request_route(uint16_t dest);

// Обработка запроса маршрута
Status_t routing_handle_route_request(uint16_t from, uint16_t dest);

// ============================================================================
// АЛГОРИТМ ДЕЙКСТРЫ
// ============================================================================

// Вычисление оптимальных маршрутов
Status_t routing_compute_dijkstra(uint16_t start_node);

// Обновление качества канала
Status_t routing_update_channel_quality(uint16_t node, uint8_t quality);

// ============================================================================
// УЗЛЫ СЕТИ
// ============================================================================

// Добавление информации об узле
Status_t routing_add_node(const NodeInfo_t* node);

// Поиск узла
const NodeInfo_t* routing_find_node(uint16_t node_id);

// Рассылка информации о своем узле
Status_t routing_broadcast_node_info(void);

// Получение списка узлов
uint8_t routing_get_node_count(void);

#ifdef __cplusplus
}
#endif