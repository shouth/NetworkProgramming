#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 1024

int get_rfc3986_host(char *uri, char *result)
{
    char *begin = strstr(uri, "://");
    if (begin == NULL) return -1;
    begin += 3;

    char *end = strstr(begin, "/");
    if (end == NULL) end = strstr(begin, "?");
    if (end == NULL) end = strstr(begin, "#");
    if (end == NULL) end = begin + strlen(begin);

    char *itr = end;
    while (itr != begin && *itr != ':') itr--;
    if (begin != itr) end = itr;

    if (*begin == '[' && *(end - 1) == ']') begin++, end--;

    size_t size = end - begin;
    strncpy(result, begin, size);
    return size;
}

int main(int argc, char **argv)
{
    char *content_uri = argv[1];
    char content_host_name[256];
    if (get_rfc3986_host(content_uri, content_host_name) == -1) {
        fprintf(stderr, "get_rfc3986_host()");
        exit(EXIT_FAILURE);
    }

    char *server_host_name;
    int server_port;

    int use_proxy = argc > 2;
    if (use_proxy) {
        server_host_name = argv[2];
        server_port = atoi(argv[3]);
    } else {
        server_host_name = content_host_name;
        server_port = 80;
    }

    struct hostent* server_host;
    struct sockaddr_in server_adrs;

    int tcpsock;

    char k_buf[BUFSIZE], s_buf[BUFSIZE], r_buf[BUFSIZE];
    int strsize;

    if ((server_host = gethostbyname(server_host_name)) == NULL) {
        fprintf(stderr, "gethostbyname()");
        exit(EXIT_FAILURE);
    }

    memset(&server_adrs, 0, sizeof(server_adrs));
    server_adrs.sin_family = AF_INET;
    server_adrs.sin_port = htons(server_port);
    memcpy(&server_adrs.sin_addr, server_host->h_addr_list[0], server_host->h_length);

    if ((tcpsock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "socket()");
        exit(EXIT_FAILURE);
    }

    if (connect(tcpsock, (struct sockaddr*)&server_adrs, sizeof(server_adrs)) < 0) {
        fprintf(stderr, "connect");
        exit(EXIT_FAILURE);
    }

    s_buf[0] = 0;
    strsize = 0;
    strsize += snprintf(k_buf, BUFSIZE, "GET %s HTTP/1.1\r\n", content_uri);
    strncat(s_buf, k_buf, BUFSIZE);
    strsize += snprintf(k_buf, BUFSIZE, "Host: %s\r\n\r\n", content_host_name);
    strncat(s_buf, k_buf, BUFSIZE);

    printf("%s", s_buf);

    if (send(tcpsock, s_buf, strsize + 1, 0) == -1) {
        fprintf(stderr, "send()");
        exit(EXIT_FAILURE);
    }

    if ((strsize = recv(tcpsock, r_buf, BUFSIZE - 1, 0)) == -1) {
        fprintf(stderr, "recv()");
        exit(EXIT_FAILURE);
    }
    r_buf[strsize] = '\0';

    printf("%s", r_buf);

    close(tcpsock);
    exit(EXIT_SUCCESS);
}
