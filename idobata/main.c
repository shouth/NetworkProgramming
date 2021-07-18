#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "mynet.h"
#include "logger.h"
#include "idobata_client.h"
#include "idobata_server.h"

#define BUFSIZE 1024

int idobata_helo(int sock, struct sockaddr *addr)
{
    char buf[BUFSIZE];
    size_t strsize = snprintf(buf, BUFSIZE, "HELO");
    Sendto(sock, buf, strsize, 0, addr, sizeof(struct sockaddr));
}

int init_broadcast_udpclient(in_addr_t addr, in_port_t port, struct sockaddr_in *ret)
{
    memset(ret, 0, sizeof(struct sockaddr_in));
    ret->sin_family = AF_INET;
    ret->sin_port = htons(port);
    ret->sin_addr.s_addr = htonl(addr);

    int broadcast_sw = 1;
    int sock = init_udpclient();
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcast_sw, sizeof(broadcast_sw)) == -1) {
        exit_errmesg("setsockopt()");
    }
    return sock;
}

int idobata_lookup(in_port_t port, struct sockaddr *server_addr)
{
    struct sockaddr_in broadcast_addr;

    int sock = init_broadcast_udpclient(INADDR_BROADCAST, port, &broadcast_addr);

    fd_set mask, readfds;
    FD_ZERO(&mask);
    FD_SET(sock, &mask);

    struct timeval timeout;

    char buf[BUFSIZE];

    for (size_t i = 0; i < 3; i++) {
        LOG_INFO("[%d/3] Trying to find server...", i + 1);

        idobata_helo(sock, (struct sockaddr *) &broadcast_addr);

        readfds = mask;
        timeout.tv_sec = 5, timeout.tv_usec = 0;

        if (select(sock + 1, &readfds, NULL, NULL, &timeout) != 0) {
            if (FD_ISSET(sock, &readfds)) {
                socklen_t socklen = sizeof(struct sockaddr);
                size_t strsize = Recvfrom(sock, buf, BUFSIZE, 0, server_addr, &socklen);
                if (strsize > 0) strsize--;
                buf[strsize] = '\0';

                if (strncmp(buf, "HERE", 4) == 0) {
                    close(sock);
                    return 0;
                }
            }
        } else {
            LOG_INFO("HELO timeout.");
        }
    }

    close(sock);
    return -1;
}

int main(int argc, char **argv)
{
    idobata_log_level(TRACE);
    LOG_INFO("Educational Idobata System");

    LOG_INFO("Finding server...");
    struct sockaddr_in server_addr;
    if (idobata_lookup(50001, (struct sockaddr *) &server_addr) != -1) {
        LOG_INFO("Server found. IP address of server is %s", inet_ntoa(server_addr.sin_addr));
        LOG_INFO("Initiating as client.");
        idobata_client((struct sockaddr *) &server_addr, "shouth");
    } else {
        LOG_INFO("Cannot find server.");
        LOG_INFO("Initiating as server.");
        idobata_server(50001, 10, "server");
    }
}
