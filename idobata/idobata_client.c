#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "idobata_client.h"
#include "mynet.h"

#define BUFSIZE 1024

static int idobata_join(int sock, const char *name)
{
    LOG_TRACE("JOIN; username = %s", name);

    char buf[BUFSIZE];
    int size = snprintf(buf, BUFSIZE, "JOIN %s", name);
    return send(sock, buf, size, 0);
}

static void idobata_mesg(const char *msg)
{
    LOG_TRACE("MESG; message = %s", msg + 5);

    printf("%s\n", msg + 5);
    fflush(stdout);
}

static int idobata_post(int sock, const char *msg)
{
    LOG_TRACE("POST; message = %s", msg);

    char buf[BUFSIZE];
    int size = snprintf(buf, BUFSIZE, "POST %s", msg);
    return send(sock, buf, size, 0);
}

static int idobata_quit(int sock)
{
    LOG_TRACE("QUIT;");

    char buf[BUFSIZE];
    int size = snprintf(buf, BUFSIZE, "QUIT");
    return send(sock, buf, size, 0);
}

int idobata_client(struct sockaddr *server_addr, const char *username)
{
    LOG_INFO("Joining server...");

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    connect(sock, server_addr, sizeof(struct sockaddr));
    idobata_join(sock, username);

    LOG_INFO("Joined server successfully.");

    fd_set mask, readfds;
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(sock, &mask);

    struct timeval timeout;

    char buf[BUFSIZE];

    while (1) {
        readfds = mask;
        timeout.tv_sec = timeout.tv_usec = 0;

        select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(0, &readfds)) {
            LOG_DEBUG("Message received from stdio.");

            if (fgets(buf, BUFSIZE - 1, stdin) == NULL) {
                LOG_TRACE("Input is empty.");
                idobata_quit(sock);
                LOG_INFO("Good-bye.");
                exit(EXIT_SUCCESS);
            } else {
                size_t strsize = strcspn(buf, "\r\n");
                buf[strsize] = '\0';
                LOG_TRACE("Input: %s", buf);

                if (strcmp(buf, "QUIT") == 0) {
                    idobata_quit(sock);
                } else {
                    idobata_post(sock, buf);
                }
            }
        }

        if (FD_ISSET(sock, &readfds)) {
            LOG_DEBUG("Message received from server.");

            ssize_t strsize;
            if ((strsize = recv(sock, buf, BUFSIZE - 1, 0)) == 0) {
                LOG_TRACE("Message is empty.");
                LOG_INFO("Server closed connection.");
                LOG_INFO("Good-bye.");
                exit(EXIT_SUCCESS);
            } else {
                buf[strsize] = '\0';
                LOG_TRACE("Message: %s", buf);

                if (strncmp(buf, "MESG", 4) == 0) {
                    idobata_mesg(buf);
                } else {
                    LOG_WARN("Ill-formed response received.");
                    LOG_WARN("Response: %s", buf);
                }
            }
        }
    }
}
