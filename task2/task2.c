#include "mynet.h"
#include <stdio.h>

#define BUFSISE 1024

int main(int argc, char const *argv[])
{
    while (1) {
        int sock_listen = init_tcpserver(50000, 10);
        int sock_accepted = accept(sock_listen, NULL, NULL);
        close(sock_listen);

        char buf[BUFSISE];
        int strsize;
        FILE *fp;

        while (1) {
            send(sock_accepted, "> ", 2, 0);
            if ((strsize = recv(sock_accepted, buf, BUFSISE, 0)) == -1) {
                exit_errmesg("recv()");
            }

            buf[strsize] = 0;

            if (strcmp(buf, "list") == 0) {
                fp = popen("ls ~/Workspace", "r");
                fgets(buf, BUFSISE, fp);
                send(sock_accepted, buf, strlen(buf), 0);
            }

            if (strncmp(buf, "type", 4) == 0) {
                char *cur = buf + 4;
                cur += strspn(cur, " ");
                int size = strcspn(cur, " ");

                char filename[256];
                strncpy(filename, cur, size);
            }

            if (strcmp(buf, "exit") == 0) break;
        }

        close(sock_accepted);
    }

    return 0;
}
