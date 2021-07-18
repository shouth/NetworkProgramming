#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "idobata_server.h"
#include "logger.h"
#include "mynet.h"

#define BUFSIZE 1024

typedef struct {
    int echo_sock;
    int connect_sock;
    client_info head;
    fd_set mask;
    int max_fd;
} server_context;

static void update_max_fd(server_context *cxt)
{
    cxt->max_fd = cxt->echo_sock > cxt->connect_sock ? cxt->echo_sock : cxt->connect_sock;
    client_info *info;
    list_for_each(info, &cxt->head.node, client_info, node) {
        if (cxt->max_fd < info->sock) cxt->max_fd = info->sock;
    }
}

static void init_server_context(server_context *cxt, int echo_sock, int connect_sock, const char *username)
{
    cxt->echo_sock = echo_sock;
    cxt->connect_sock = connect_sock;
    list_init(&cxt->head.node);
    update_max_fd(cxt);
    strncpy(cxt->head.name, username, sizeof(cxt->head.name));

    FD_ZERO(&cxt->mask);
    FD_SET(0, &cxt->mask);
    FD_SET(cxt->echo_sock, &cxt->mask);
    FD_SET(cxt->connect_sock, &cxt->mask);
}

static void free_client_info(server_context *cxt, client_info *info)
{
    FD_CLR(info->sock, &cxt->mask);
    update_max_fd(cxt);
    list_remove(&info->node);
    close(info->sock);
    free(info);
}

static void do_mesg(client_info_list *head, client_info *sender, const char *msg)
{
    LOG_TRACE("MESG; sender = %s, message = %s", sender->name, msg);
    if (strlen(msg) > 488) {
        LOG_INFO("Message will be trimmed because it exceeds 488 bytes.");
    }

    char buf[BUFSIZE];
    size_t strsize = snprintf(buf, BUFSIZE, "MESG [%s] %.488s\n", sender->name, msg);

    client_info *info;
    list_for_each(info, head, client_info, node) {
        if (info != sender) {
            if (send(info->sock, buf, strsize, 0) == -1) {
                LOG_ERROR("Failed to send MESG.");
            }
        }
    }

    if (head != &sender->node) printf("%s", buf + 5);
}

static void proc_stdio(server_context *cxt)
{
    LOG_DEBUG("Message received from stdio.");

    char buf[BUFSIZE];
    fgets(buf, BUFSIZE - 1, stdin);
    size_t strsize = strlen(buf) - 1;
    buf[strsize] = '\0';

    LOG_TRACE("Message: %s", buf);

    do_mesg(&cxt->head.node, &cxt->head, buf);
}

static void proc_echo(server_context *cxt)
{
    LOG_DEBUG("Message received from echo socket.");

    char buf[BUFSIZE];
    struct sockaddr_in addr;
    socklen_t from_len = sizeof(addr);
    size_t strsize = Recvfrom(cxt->echo_sock, buf, BUFSIZE - 1, 0, (struct sockaddr *) &addr, &from_len);
    buf[strsize] = '\0';

    LOG_TRACE("Message: %s", buf);

    if (strncmp(buf, "HELO", 4) == 0) {
        LOG_TRACE("HELO; Sender IP address = %s", inet_ntoa(addr.sin_addr));
        LOG_TRACE("HERE;");
        size_t strsize = snprintf(buf, BUFSIZE, "HERE");
        Sendto(cxt->echo_sock, buf, strsize, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr));
    }
}

static void proc_connect(server_context *cxt)
{
    LOG_DEBUG("Message received from connect socket.");

    int accept_sock = accept(cxt->connect_sock, NULL, NULL);
    client_info *info = (client_info *) malloc(sizeof(client_info));
    info->state = JOINING;
    info->sock = accept_sock;
    info->last_update = time(NULL);
    list_insert(cxt->head.node.prev, &info->node);
    FD_SET(accept_sock, &cxt->mask);
    update_max_fd(cxt);

    LOG_DEBUG("Inserted new client information.");
}

static void proc_client(server_context *cxt, client_info *info)
{
    LOG_DEBUG("Message received from client.");

    char buf[BUFSIZE];
    ssize_t strsize = recv(info->sock, buf, BUFSIZE - 1, 0);
    buf[strsize] = '\0';

    LOG_TRACE("message = %s, size = %d", buf, strsize);

    if (strncmp(buf, "JOIN", 4) == 0) {
        LOG_TRACE("JOIN; user = %s", buf + 5);
        if (strlen(buf + 5) > 15) {
            LOG_INFO("User %.15s... tried to JOIN, but server refused because name of this user is greater than 15 characters.", buf + 5);
            free_client_info(cxt, info);
            return;
        }
        info->state = JOINED;
        strncpy(info->name, buf + 5, sizeof(info->name));
    }

    if (strncmp(buf, "POST", 4) == 0) {
        LOG_TRACE("POST; sender = %s, message = %s", info->name, buf + 5);
        do_mesg(&cxt->head.node, info, buf + 5);
    }

    if (strncmp(buf, "QUIT", 4) == 0 || strsize == 0) {
        LOG_TRACE("QUIT; user = %s", info->name);
        free_client_info(cxt, info);
    }
}

int idobata_server(in_port_t port, size_t capacity, const char *username)
{
    int echo_sock = init_udpserver(port);
    int connect_sock = init_tcpserver(port, capacity);
    server_context cxt;
    init_server_context(&cxt, echo_sock, connect_sock, username);

    LOG_DEBUG("Server initialized.");

    while (1) {
        fd_set readfds = cxt.mask;
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };
        select(cxt.max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(0, &readfds)) {
            proc_stdio(&cxt);
        }

        if (FD_ISSET(cxt.echo_sock, &readfds)) {
            proc_echo(&cxt);
        }

        if (FD_ISSET(cxt.connect_sock, &readfds)) {
            proc_connect(&cxt);
        }

        client_info *info;
        list_for_each(info, &cxt.head.node, client_info, node) {
            if (FD_ISSET(info->sock, &readfds)) {
                proc_client(&cxt, info);
                info->last_update = time(NULL);
            } else {
                time_t elapsed = time(NULL) - info->last_update;
                if (info->state == JOINED) {
                    if (elapsed > 600) {
                        LOG_INFO("Invalidated user %s because this user has been inactive for 600 seconds.", info->name);
                        free_client_info(&cxt, info);
                    }
                } else {
                    if (elapsed > 60) {
                        LOG_INFO("Invalidated anonymous user because this user did not JOIN within 60 seconds of connecting.", info->name);
                        free_client_info(&cxt, info);
                    }
                }
            }
        }
    }
}
