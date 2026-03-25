#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t id;
    char name[MAX_USERNAME_LEN];
    uint32_t last_seen;
    uint8_t quality;
    bool is_server;
} KnownNode_t;

typedef struct {
    bool connected_to_server;
    uint32_t last_beacon;
    uint32_t last_activity;
    KnownNode_t known_nodes[MAX_KNOWN_NODES];
    uint8_t node_count;
    char username[MAX_USERNAME_LEN];
    bool logged_in;
    BbsScreen_t current_screen;
    uint16_t seq_counter;
} ClientState_t;

Status_t client_init(void);
Status_t client_loop(void);
Status_t client_handle_packet(const uint8_t* buffer, size_t len);
Status_t client_send_beacon(void);
Status_t client_send_chat(const char* msg);
Status_t client_request_mail(uint16_t target_id);
Status_t client_ping_server(void);
Status_t client_navigate(BbsScreen_t screen);

ClientState_t* client_get_state(void);
bool client_is_connected(void);
KnownNode_t* client_find_node(uint16_t node_id);

#ifdef __cplusplus
}
#endif