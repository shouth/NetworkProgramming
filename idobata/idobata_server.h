#ifndef IDOBATA_SERVER_H
#define IDOBATA_SERVER_H

#include <arpa/inet.h>

#include "list.h"

typedef enum {
    JOINING,
    JOINED
} client_state;

typedef struct {
    char name[256];
    int sock;
    client_state state;
    time_t last_update;
    list_node node;
} client_info;

typedef list_node client_info_list;

int idobata_server(in_port_t port, size_t capacity, const char *username);

#endif
