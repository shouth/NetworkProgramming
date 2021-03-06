#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "mynet.h"
#include "logger.h"
#include "idobata_client.h"
#include "idobata_server.h"

#define BUFSIZE 1024
#define TIMEOUT_SEC 5

static void show_help(const char *command_name)
{
    printf(
        "Idobata\n"
        "A communication system for education purposes.\n"
        "\n"
        "USAGE\n"
        "  %s -u <user-name> [-p <port-number>] [-l <log-level>] [-c] [-h]\n"
        "\n"
        "OPTIONS\n"
        "  -u <user-name>       User name\n"
        "  -p <port-number>     Port number of server. Default to 50001\n"
        "  -l <log-level>       Log level. Default to INFO\n"
        "  -c                   Disable output color\n"
        "  -h                   Show this help\n"
        , command_name);
}

static int init_broadcast_udpclient(in_addr_t addr, in_port_t port, struct sockaddr_in *ret)
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

static int idobata_lookup(in_port_t port, struct sockaddr *server_addr)
{
    struct sockaddr_in broadcast_addr;

    int sock = init_broadcast_udpclient(INADDR_BROADCAST, port, &broadcast_addr);

    fd_set mask, readfds;
    FD_ZERO(&mask);
    FD_SET(sock, &mask);

    char buf[BUFSIZE];

    size_t i;
    for (i = 0; i < 3; i++) {
        LOG_DEBUG("[%d/3] Finding server...", i + 1);

        size_t strsize = snprintf(buf, BUFSIZE, "HELO");
        Sendto(sock, buf, strsize, 0, (struct sockaddr *) &broadcast_addr, sizeof(struct sockaddr));

        readfds = mask;
        struct timeval timeout = { .tv_sec = TIMEOUT_SEC, .tv_usec = 0 };

        if (select(sock + 1, &readfds, NULL, NULL, &timeout) != 0) {
            if (FD_ISSET(sock, &readfds)) {
                socklen_t socklen = sizeof(struct sockaddr);
                size_t strsize = Recvfrom(sock, buf, BUFSIZE, 0, server_addr, &socklen);
                buf[strsize] = '\0';

                LOG_TRACE("Received from server: %s", buf);
                if (strncmp(buf, "HERE", 4) == 0) {
                    close(sock);
                    return 0;
                } else {
                    LOG_DEBUG("Server responded, but responce is ill-formed.");
                }
            }
        } else {
            LOG_DEBUG("HELO timeout.");
        }
    }

    close(sock);
    return -1;
}

int main(int argc, char **argv)
{
    in_port_t port = 50001;
    char username[256];
    int init_username = 0;

    int opt;
    while ((opt = getopt(argc, argv, "u:p:l:ch")) != -1) {
        switch (opt) {
        case 'c':
            idobata_log_colored(0);
            break;

        case 'l':
            if (strcmp(optarg, "NONE") == 0 || strcmp(optarg, "none") == 0) idobata_log_level(100);
            if (strcmp(optarg, "FATAL") == 0 || strcmp(optarg, "fatal") == 0) idobata_log_level(FATAL);
            if (strcmp(optarg, "ERROR") == 0 || strcmp(optarg, "error") == 0) idobata_log_level(ERROR);
            if (strcmp(optarg, "WARN") == 0 || strcmp(optarg, "warn") == 0) idobata_log_level(WARN);
            if (strcmp(optarg, "INFO") == 0 || strcmp(optarg, "info") == 0) idobata_log_level(INFO);
            if (strcmp(optarg, "DEBUG") == 0 || strcmp(optarg, "debug") == 0) idobata_log_level(DEBUG);
            if (strcmp(optarg, "TRACE") == 0 || strcmp(optarg, "trace") == 0) idobata_log_level(TRACE);
            break;

        case 'u':
            if (strlen(optarg) > 15) {
                LOG_ERROR("User name needs to be less than or equal to 15 characters.");
                exit(EXIT_FAILURE);
            }
            strncpy(username, optarg, sizeof(username));
            init_username = 1;
            break;

        case 'p':
            port = atoi(optarg);
            break;

        case 'h':
            show_help(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    if (!init_username) {
        show_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    LOG_INFO("Educational Idobata Communication System");

    LOG_INFO("Finding server...");
    struct sockaddr_in server_addr;
    if (idobata_lookup(port, (struct sockaddr *) &server_addr) != -1) {
        LOG_INFO("Server found. Server IP address = %s", inet_ntoa(server_addr.sin_addr));
        LOG_INFO("Running as client.");
        idobata_client((struct sockaddr *) &server_addr, username);
    } else {
        LOG_INFO("Cannot find server.");
        LOG_INFO("Running as server.");
        idobata_server(port, 10, "server");
    }
}
