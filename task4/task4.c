#include "mynet.h"
#include <sys/wait.h>
#include <pthread.h>

#define BUFSIZE 50 /* バッファサイズ */

void echo(int sock_listen);
void *echo_thread(void *arg);

int main(int argc, char *argv[])
{
    int port_number;
    int parallel_type;
    int connection_limit;

    int sock_listen;

    pid_t child;

    int *tharg;
    pthread_t tid;

    int i;

    if (argc != 4) {
        fprintf(stderr,
            "Usage: %s port_number parallel_type connection_limit\n"
            "    port_number : The port number to listen.\n"
            "    parallel_type : If 0 is specified, fork() is used. If 1 is specified, thread is used.\n"
            "    connection_limit : The maximum number of connection to process simultaneously.\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }

    port_number = atoi(argv[1]);
    parallel_type = atoi(argv[2]);
    connection_limit = atoi(argv[3]);

    sock_listen = init_tcpserver(port_number, 5);

    if (parallel_type == 0) {
        for (i = 0; i < connection_limit; i++) {
            if ((child = fork()) != 0) {
                echo(sock_listen);
                // echo()は処理を返さないので子プロセスがこれ以上fork()を行うことはない
            } else if (child < 0) {
                exit_errmesg("fork()");
            }
        }
    } else if (parallel_type == 1) {
        for (i = 0; i < connection_limit; i++) {
            if ((tharg = (int *) malloc(sizeof(int))) == NULL) {
                exit_errmesg("malloc()");
            }
            *tharg = sock_listen;
            // 今回はthread_tを複数の処理で取りまわす必要がないため，変数tidは
            // pthread_createを呼び出すためのプレースホルダとしての使用にとどまっている
            if (pthread_create(&tid, NULL, echo_thread, (void *) tharg) != 0) {
                exit_errmesg("pthread_create()");
            }
        }
    }

    for (;;);
}

int thread_id = 0;

void echo(int sock_listen)
{
    int sock_accepted;
    char buf[BUFSIZE];
    int strsize;

    // スレッドで並行処理を行っているときに出力を見分けるための変数
    int this_thread_id = thread_id++;

    for (;;) {
        sock_accepted = accept(sock_listen, NULL, NULL);
        printf("client is accepted [pid = %d, thread_id = %d]\n", getpid(), this_thread_id);
        do {
            if ((strsize = recv(sock_accepted, buf, BUFSIZE, 0)) == -1) {
                exit_errmesg("recv()");
            }

            if (send(sock_accepted, buf, strsize, 0) == -1) {
                exit_errmesg("send()");
            }
        } while (buf[strsize - 1] != '\n');
        close(sock_accepted);
    }
}

void *echo_thread(void *arg)
{
    int sock_listen = *((int *) arg);
    free(arg);

    pthread_detach(pthread_self());

    echo(sock_listen);

    return (NULL);
}
