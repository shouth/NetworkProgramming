#include "mynet.h"
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

#define S_BUFSIZE 512 /* 送信用バッファサイズ */
#define R_BUFSIZE 512 /* 受信用バッファサイズ */
#define TIMEOUT_SEC 10

static char s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int r;
    if ((r = accept(s, addr, addrlen)) == -1) {
        exit_errmesg("accept()");
    }
    return (r);
}

int Send(int s, void *buf, size_t len, int flags)
{
    int r;
    if ((r = send(s, buf, len, flags)) == -1) {
        exit_errmesg("send()");
    }
    return (r);
}

int Recv(int s, void *buf, size_t len, int flags)
{
    int r;
    if ((r = recv(s, buf, len, flags)) == -1) {
        exit_errmesg("recv()");
    }
    return (r);
}

void set_sockaddr_in_broadcast(struct sockaddr_in *server_adrs, in_port_t port_number)
{
    memset(server_adrs, 0, sizeof(struct sockaddr_in));
    server_adrs->sin_family = AF_INET;
    server_adrs->sin_port = htons(port_number);
    server_adrs->sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

int init_broadcast_udpclient(struct sockaddr_in *adrs)
{
    int broadcast_sw = 1;
    int sock = init_udpclient();
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcast_sw, sizeof(broadcast_sw)) == -1) {
        exit_errmesg("setsockopt()");
    }
    return sock;
}

int init_tcpclient_sockaddr(const struct sockaddr *addr, size_t len)
{
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        exit_errmesg("socket()");
    }
    if (connect(sock, addr, len) == -1) {
        exit_errmesg("connect()");
    }
    return sock;
}

int idobata_lookup(in_port_t port, const char *user_name)
{
    struct sockaddr_in broadcast_adrs;
    struct sockaddr_in server_addr;
    socklen_t server_addr_len;

    int sock;
    fd_set mask, readfds;
    struct timeval timeout;

    int strsize;
    int i;

    set_sockaddr_in_broadcast(&broadcast_adrs, port);
    sock = init_broadcast_udpclient(&broadcast_adrs);

    FD_ZERO(&mask);
    FD_SET(sock, &mask);

    printf("Finding server...\n");
    fflush(stdout);

    for (i = 0; i < 3; i++) {
        strsize = sprintf(s_buf, "HELO");
        Sendto(sock, s_buf, strsize, 0, (struct sockaddr *) &broadcast_adrs, sizeof(broadcast_adrs));

        readfds = mask;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        if (select(sock + 1, &readfds, NULL, NULL, &timeout) == 0) {
            if (i < 2) {
                printf("Timeout. Retrying...\n");
                continue;
            } else {
                fprintf(stderr, "Failed to find server.");
                exit(EXIT_FAILURE);
            }
        }

        server_addr_len = sizeof(server_addr);
        strsize = Recvfrom(sock, r_buf, R_BUFSIZE, 0, (struct sockaddr *) &server_addr, &server_addr_len);

        if (strcmp(r_buf, "HERE") == 0) {
            printf("Server found. IP: %s\n", inet_ntoa(server_addr.sin_addr));
            fflush(stdout);
            break;
        }

        if (i < 2) {
            printf("Received ill-formed response. Retrying...\n");
            continue;
        } else {
            fprintf(stderr, "Failed to find server.");
            exit(EXIT_FAILURE);
        }
    }
    close(sock);

    sock = init_tcpclient_sockaddr((struct sockaddr *) &server_addr, server_addr_len);
    strsize = sprintf(s_buf, "JOIN %s", user_name);
    Send(sock, s_buf, strsize, 0);

    return sock;
}

int main(int argc, char *argv[])
{
    int sock;
    fd_set mask, readfds;
    struct timeval timeout;

    int strsize;
    int i;

    char *user_name;
    in_port_t port;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s user_name [port]\n", argv[0]);
        exit(1);
    }

    user_name = argv[1];

    if (strlen(user_name) > 15) {
        fprintf(stderr, "The length of user_name must be less than or equal to 15 characters.");
        exit(1);
    }

    port = argc > 2 ? atoi(argv[2]) : 50001;
    sock = idobata_lookup(port, user_name);

    FD_ZERO(&mask);
    FD_SET(sock, &mask);
    FD_SET(0, &mask);

    for (;;) {
        readfds = mask;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(0, &readfds)) {
            fgets(r_buf, S_BUFSIZE, stdin);
            strsize = strlen(r_buf) - 1;
            r_buf[strsize] = '\0';

            if (strcmp(r_buf, "QUIT") == 0) {
                Send(sock, r_buf, strsize, 0);
                break;
            }

            strsize = snprintf(s_buf, S_BUFSIZE, "POST %s", r_buf);
            Send(sock, s_buf, strsize, 0);
        }

        if (FD_ISSET(sock, &readfds)) {
            strsize = Recv(sock, r_buf, R_BUFSIZE - 1, 0);

            if (strsize <= 0) {
                printf("Disconnected.");
                fflush(stdout);
                exit(0);
            }

            if (strncmp(r_buf, "MESG", 4) == 0) {
                r_buf[strsize] = '\0';
                printf("%s", r_buf + 5);
                fflush(stdout);
            }
        }
    }

    close(sock);
    exit(EXIT_SUCCESS);
}
