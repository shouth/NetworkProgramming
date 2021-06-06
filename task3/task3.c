#include "mynet.h"
#include <stdio.h>

#define BUFSIZE 1024

int main(int argc, char const *argv[])
{
    in_port_t port;
    if (argc == 1) {
        port = 50000;
    } else {
        port = atoi(argv[1]);
    }

    int sock_listen = init_tcpserver(port, 10);

    while (1) {
        int sock_accepted = accept(sock_listen, NULL, NULL);

        char buf[BUFSIZE];
        int strsize;
        FILE *fp;

        sprintf(buf, "password: ");
        send(sock_accepted, buf, strlen(buf), 0);

        if ((strsize = recv(sock_accepted, buf, BUFSIZE, 0)) == -1) {
            exit_errmesg("recv()");
        }

        buf[strsize] = 0;

        const char *password = "password";
        if (strncmp(buf, password, strlen(password)) != 0) {
            sprintf(buf, "Sorry, password is incorrect.\r\n");
            send(sock_accepted, buf, strlen(buf), 0);
            close(sock_accepted);
            continue;
        }

        sprintf(buf, "Welcome, authorized personel.\r\n");
        send(sock_accepted, buf, strlen(buf), 0);

        while (1) {
            send(sock_accepted, "> ", 2, 0);
            if ((strsize = recv(sock_accepted, buf, BUFSIZE, 0)) == -1) {
                exit_errmesg("recv()");
            }

            buf[strsize] = 0;

            if (strncmp(buf, "list", 4) == 0) {
                fp = popen("ls ~/work", "r");
                fgets(buf, BUFSIZE, fp);

                while (!feof(fp)) {
                    fgets(buf, BUFSIZE, fp);
                    send(sock_accepted, buf, strlen(buf), 0);
                }

                if (fclose(fp) != 0) {
                    sprintf(buf, "Directory not found.\r\n");
                    send(sock_accepted, buf, strlen(buf), 0);
                }
            } else if (strncmp(buf, "type", 4) == 0) {
                char *cur = buf + 4;
                cur += strspn(cur, " ");
                int size = strcspn(cur, " \r\n");
                cur[size] = 0;

                if (strncmp(cur, "..", 2) == 0 || strstr(cur, "../") != NULL) {
                    sprintf(buf, "Sorry, using `..` as parent directory is not allowed.\r\n");
                    send(sock_accepted, buf, strlen(buf), 0);
                    continue;
                }

                char command[256];
                sprintf(command, "cat ~/work/%s", cur);
                fp = popen(command, "r");
                fgets(buf, BUFSIZE, fp);

                while (!feof(fp)) {
                    fgets(buf, BUFSIZE, fp);
                    send(sock_accepted, buf, strlen(buf), 0);
                }

                if (fclose(fp) != 0) {
                    sprintf(buf, "No such file.\r\n");
                    send(sock_accepted, buf, strlen(buf), 0);
                }
            } else if (strncmp(buf, "exit", 4) == 0) {
                sprintf(buf, "Goodbye.\r\n");
                send(sock_accepted, buf, strlen(buf), 0);
                break;
            } else {
                sprintf(buf, "Command not found.\r\n");
                send(sock_accepted, buf, strlen(buf), 0);
            }
        }

        close(sock_accepted);
    }

    return 0;
}
