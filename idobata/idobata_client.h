#ifndef IDOBATA_CLIENT_H
#define IDOBATA_CLIENT_H

#include <stdio.h>
#include <sys/socket.h>

#include "logger.h"

int idobata_client(struct sockaddr *server_addr, const char *username);

#endif
