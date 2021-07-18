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

#define S_BUFSIZE 512 /* 送信用バッファサイズ */
#define R_BUFSIZE 512 /* 受信用バッファサイズ */
#define TIMEOUT_SEC 10

static char s_buf[S_BUFSIZE], r_buf[R_BUFSIZE];

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

int find_server(in_port_t port, const char *user_name)
{
    const char *servername = "172.25.207.171";
    struct hostent *server_host;
    struct sockaddr_in server_adrs;
    int sock;

    /* サーバ名をアドレス(hostent構造体)に変換する */
    if ((server_host = gethostbyname(servername)) == NULL) {
        exit_errmesg("gethostbyname()");
    }

    /* サーバの情報をsockaddr_in構造体に格納する */
    memset(&server_adrs, 0, sizeof(server_adrs));
    server_adrs.sin_family = AF_INET;
    server_adrs.sin_port = htons(port);
    memcpy(&server_adrs.sin_addr, server_host->h_addr_list[0], server_host->h_length);

    idobata_client((struct sockaddr *) &server_adrs, user_name);

    return sock;
}

int main(int argc, char **argv)
{
    idobata_log_level(TRACE);
    LOG_INFO("Educational Idobata System");

    find_server(50001, "shouth");
}
