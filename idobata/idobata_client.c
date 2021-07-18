#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "idobata_client.h"
#include "mynet.h"

#define BUFSIZE 1024

typedef struct {
    int sock;
    char name[256];
    fd_set mask;
    int max_fd;
} client_context;

static void init_client_context(client_context *cxt, int sock, const char *name)
{
    cxt->sock = sock;
    strncpy(cxt->name, name, sizeof(cxt->name));
    cxt->max_fd = sock;

    FD_ZERO(&cxt->mask);
    FD_SET(0, &cxt->mask);
    FD_SET(cxt->sock, &cxt->mask);
}

static void do_join(client_context *cxt)
{
    LOG_TRACE("JOIN; username = %s", cxt->name);

    char buf[BUFSIZE];
    int size = snprintf(buf, BUFSIZE, "JOIN %s", cxt->name);
    if (send(cxt->sock, buf, size, 0) == -1) {
        LOG_FATAL("Failed to JOIN. exit.");
        exit(EXIT_FAILURE);
    }
}

static void do_quit(client_context *cxt)
{
    LOG_TRACE("QUIT;");

    char buf[BUFSIZE];
    size_t strsize = snprintf(buf, BUFSIZE, "QUIT");
    if (send(cxt->sock, buf, strsize, 0) == -1) {
        LOG_FATAL("Failed to QUIT. exit.");
        exit(EXIT_FAILURE);
    }
}

static void proc_stdio(client_context *cxt)
{
    LOG_DEBUG("Message received from stdio.");

    char r_buf[BUFSIZE], s_buf[BUFSIZE];

    if (fgets(r_buf, BUFSIZE - 1, stdin) == NULL) {
        LOG_TRACE("Input is empty.");
        do_quit(cxt);
        LOG_INFO("Good-bye.");
        exit(EXIT_SUCCESS);
    } else {
        size_t strsize = strcspn(r_buf, "\r\n");
        r_buf[strsize] = '\0';
        if (strsize > 488) {
            LOG_INFO("Message will be trimmed because it exceeds 488 bytes.");
        }
        LOG_TRACE("Input: %s", r_buf);

        if (strcmp(r_buf, "QUIT") == 0) {
            do_quit(cxt);
        } else {
            LOG_TRACE("POST; message = %s", r_buf);
            int size = snprintf(s_buf, BUFSIZE, "POST %.488s", r_buf);
            if (send(cxt->sock, s_buf, size, 0) == -1) {
                LOG_ERROR("Failed to POST.");
            }
        }
    }
}

static void proc_server(client_context *cxt)
{
    LOG_DEBUG("Message received from server.");

    char buf[BUFSIZE];
    ssize_t strsize;

    if ((strsize = recv(cxt->sock, buf, BUFSIZE - 1, 0)) == 0) {
        LOG_TRACE("Message is empty.");
        LOG_INFO("Server closed connection.");
        LOG_INFO("Good-bye.");
        exit(EXIT_SUCCESS);
    } else {
        buf[strsize] = '\0';
        LOG_TRACE("Message: %s", buf);

        if (strncmp(buf, "MESG", 4) == 0) {
            LOG_TRACE("MESG; message = %s", buf + 5);
            printf("%s\n", buf + 5);
            fflush(stdout);
        } else {
            LOG_WARN("Ill-formed response received.");
            LOG_WARN("Response: %s", buf);
        }
    }
}

int idobata_client(struct sockaddr *server_addr, const char *username)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        LOG_FATAL("Failed to create socket. exit.");
        exit(EXIT_FAILURE);
    }
    if (connect(sock, server_addr, sizeof(struct sockaddr)) == -1) {
        LOG_FATAL("Failed to establish connection. exit.");
        exit(EXIT_FAILURE);
    }

    client_context cxt;
    init_client_context(&cxt, sock, username);

    LOG_INFO("Client initialized.");

    LOG_INFO("Joining server...");
    do_join(&cxt);
    LOG_INFO("Joined server successfully.");

    char buf[BUFSIZE];

    while (1) {
        fd_set readfds = cxt.mask;
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

        select(cxt.max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(0, &readfds)) {
            proc_stdio(&cxt);
        }

        if (FD_ISSET(sock, &readfds)) {
            proc_server(&cxt);
        }
    }
}
