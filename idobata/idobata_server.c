#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "idobata_server.h"
#include "logger.h"
#include "mynet.h"

#define BUFSIZE 1024

static int idobata_here(int sock, struct sockaddr_in *client_addr)
{
    LOG_TRACE("HERE; client ip address = %s", inet_ntoa(client_addr->sin_addr));

    char buf[BUFSIZE];
    size_t strsize = snprintf(buf, BUFSIZE, "HERE");
    Sendto(sock, buf, strsize, 0, (struct sockaddr *) client_addr, sizeof(struct sockaddr));
}

static int idobata_join(client_info_list *node, const char *name)
{
    LOG_TRACE("JOIN; name = %s", name);

    client_info *info = list_entry(node, client_info, node);
    info->state = JOINED;
    strncpy(info->name, name + 5, sizeof(info->name));
}

static int idobata_quit(client_info_list *node)
{
    client_info *info = list_entry(node, client_info, node);
    LOG_TRACE("QUIT; user = %s", info->name);

    list_remove(node);
    close(info->sock);
    free(info);
}

static int idobata_mesg(client_info_list *head, client_info_list *sender, const char *msg, size_t size)
{
    client_info *sender_info = list_entry(sender, client_info, node);
    LOG_TRACE("MESG; sender = %s, message = %s", sender_info->name, msg);

    char buf[BUFSIZE];
    size_t strsize = snprintf(buf, BUFSIZE, "MESG [%s] %s", sender_info->name, msg);

    client_info_list *node;
    list_for(node, head) {
        if (node == sender) continue;
        client_info *info = list_entry(node, client_info, node);
        send(info->sock, buf, strsize, 0);
    }

    printf("%s\n", buf + 5);
}

static int idobata_post(client_info_list *head, client_info_list *sender, const char *msg, size_t size)
{
    client_info *sender_info = list_entry(sender, client_info, node);
    LOG_TRACE("POST; sender = %s, message = %s", sender_info->name, msg);
    idobata_mesg(head, sender, msg + 5, size - 5);
}

int idobata_server(in_port_t port, size_t capacity, const char *username)
{
    int tcp_sock = init_tcpserver(port, capacity);
    int udp_sock = init_udpserver(port);
    int max_fd = tcp_sock > udp_sock ? tcp_sock : udp_sock;

    struct timeval timeout;

    fd_set mask, readfds;
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(tcp_sock, &mask);
    FD_SET(udp_sock, &mask);

    char buf[BUFSIZE];

    client_info server;
    list_init(&server.node);
    strncpy(server.name, username, sizeof(server.name));
    client_info_list *head = &server.node;

    LOG_DEBUG("Server initialized.");

    while (1) {
        readfds = mask;
        timeout.tv_sec = timeout.tv_usec = 0;
        select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(0, &readfds)) {
            LOG_DEBUG("Message received from stdio.");

            fgets(buf, BUFSIZE - 1, stdin);
            size_t strsize = strlen(buf) - 1;
            buf[strsize] = '\0';

            LOG_TRACE("Message: %s", buf);

            idobata_mesg(head, head, buf, strsize);
        }

        if (FD_ISSET(udp_sock, &readfds)) {
            LOG_DEBUG("Message received from HERE socket.");

            struct sockaddr_in addr;
            socklen_t from_len = sizeof(addr);
            size_t strsize = Recvfrom(udp_sock, buf, BUFSIZE - 1, 0, (struct sockaddr *) &addr, &from_len);
            buf[strsize] = '\0';

            LOG_TRACE("Message: %s", buf);

            if (strncmp(buf, "HELO", 4) == 0) idobata_here(udp_sock, &addr);
        }

        if (FD_ISSET(tcp_sock, &readfds)) {
            LOG_DEBUG("Message received from JOIN socket.");

            int accept_sock = accept(tcp_sock, NULL, NULL);

            client_info *info = (client_info *) malloc(sizeof(client_info));
            info->state = JOINING;
            info->sock = accept_sock;
            info->last_update = time(NULL);
            list_insert(head->prev, &info->node);

            if (max_fd < accept_sock) max_fd = accept_sock;
            FD_SET(accept_sock, &mask);
        }

        client_info_list *node;
        list_for(node, head) {
            client_info *info = list_entry(node, client_info, node);

            if (FD_ISSET(info->sock, &readfds)) {
                LOG_DEBUG("Message received from client.");

                ssize_t strsize = recv(info->sock, buf, BUFSIZE - 1, 0);
                buf[strsize] = '\0';

                LOG_TRACE("Message: %s", buf);

                if (strncmp(buf, "JOIN", 4) == 0) {
                    idobata_join(node, buf);
                }

                if (strncmp(buf, "POST", 4) == 0) {
                    idobata_post(head, node, buf, strsize);
                }

                if (strncmp(buf, "QUIT", 4) == 0 || strsize == 0) {
                    FD_CLR(info->sock, &mask);

                    client_info_list *tmp_node;
                    max_fd = tcp_sock > udp_sock ? tcp_sock : udp_sock;
                    list_for(tmp_node, head) {
                        client_info *tmp_info = list_entry(&head, client_info, node);
                        if (max_fd < tmp_info->sock) max_fd = tmp_info->sock;
                    }
                    idobata_quit(node);
                }

                info->last_update = time(NULL);
            } else {
                time_t elapsed = time(NULL) - info->last_update;
                time_t timeout = info->state == JOINING ? 300 : 1800;
                if (elapsed > timeout) idobata_quit(node);
            }
        }
    }
}
