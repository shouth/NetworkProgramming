/**
 * このプログラムはthreadを使用します．オプション指定についてはquizの
 * サンプルプログラムから流用しています．
 *
 * 工夫した点
 * - threadを用いて，同時接続されてもそれぞれのクライアントでログインの処理を並行して行えるようにしました
 * - サーバーが起動中でかつチャットルームが満員でない場合は自由に出入りができるようにしました
 * - 誰が入退室したかを他のクライアントに通知されるようにしました
 *
 * 苦労した点
 * - 文字列処理．送信されてきたメッセージからCRを除去できていない状態でそのまま別のクライアントに送信したが
 *   ためにクライアント側でメッセージが上書きされたりなど．
 * - 接続情報の処理．独自でチャットルームの出入りを自由にしたがために，closeしたソケットに後からsendして
 *   しまったり，fd_setの変数を適切に処理できていなかったりなど．どの関数がどんな処理をするかについては特段
 *   苦労することもなかったが，とにかくその引数の取り回しで誤った処理を書いてしまうことが多かった．
 */
#include <mynet.h>
#include <pthread.h>
#include <sys/select.h>

#define SERVER_LEN 256 /* サーバ名格納用バッファサイズ */
#define DEFAULT_PORT 50000 /* ポート番号既定値 */
#define DEFAULT_NCLIENT 3 /* 省略時のクライアント数 */
#define DEFAULT_MODE 'C' /* 省略時はクライアント */

#define NAMELENGTH 20 /* 名前の最大長 */
#define BUFLEN 1000 /* 通信バッファサイズ */

#define LOGIN_THREAD_NUM 5

extern char *optarg;
extern int optind, opterr, optopt;

typedef enum {
    LOGIN, /* ログイン処理中 */
    VALID, /* 有効．ログイン済み */
    INVALID, /* 無効．使用されていない． */
} client_state;

typedef struct {
    int sock;
    client_state state;
    char name[NAMELENGTH];
} client_info;

static int N_client; /* クライアントの数 */
static client_info *Client; /* クライアントの情報 */
static int Max_sd; /* ディスクリプタ最大値 */
static char recv_buf[BUFLEN], send_buf[BUFLEN]; /* 通信用バッファ */

static fd_set mask;

/**
 * acceptをラップした関数です．処理に失敗した場合，exit()を呼び出して
 * プログラム全体を終了させます．
 */
int Accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int r;
    if ((r = accept(s, addr, addrlen)) == -1) {
        exit_errmesg("accept()");
    }
    return (r);
}

/**
 * sendをラップした関数です．処理に失敗した場合，exit()を呼び出して
 * プログラム全体を終了させます．
 */
int Send(int s, void *buf, size_t len, int flags)
{
    int r;
    if ((r = send(s, buf, len, flags)) == -1) {
        exit_errmesg("send()");
    }
    return (r);
}

/**
 * recvをラップした関数です．処理に失敗した場合，exit()を呼び出して
 * プログラム全体を終了させます．
 */
int Recv(int s, void *buf, size_t len, int flags)
{
    int r;
    if ((r = recv(s, buf, len, flags)) == -1) {
        exit_errmesg("recv()");
    }
    return (r);
}

/**
 * 現在ログイン済みの全てのクライアントに発言者を明記したメッセージを
 * 送信します．send_client_idが-1の場合はメッセージの発言者はサーバー
 * プログラム自身として扱われます．
 */
void broadcast(int send_client_id, const char *buf)
{
    int client_id;
    int strsize;
    static char msg_buf[BUFLEN];

    strsize = snprintf(msg_buf, BUFLEN, "%s> %s\n",
        send_client_id != -1 ? Client[send_client_id].name : "SERVER",
        buf);

    printf("%s", msg_buf);
    fflush(stdout);
    for (client_id = 0; client_id < N_client; client_id++) {
        /* 接続情報が有効(VALID)であるクライアントにのみ送信する． */
        if (Client[client_id].state == VALID) {
            Send(Client[client_id].sock, msg_buf, strsize, 0);
        }
    }
}

/**
 * チャットルームへのログイン処理を担当する関数です．スレッドとして起動される
 * ことが想定されています．ログインが行われるたびに誰が入室したかに関するメッセージを
 * broadcast()します．
 */
void *login_thread(void *arg)
{
    int sock_listen;
    char buf[BUFLEN];
    int strsize;
    int i;

    int client_id, sock_accepted;
    static char prompt[] = "Input your name: ";
    static char full_message[] = "Sorry, chat room is full.\n";
    static char reserved_message[] = "Sorry, name `SERVER` is reserved.\n";
    char loginname[NAMELENGTH];

    sock_listen = *((int *) arg);
    free(arg);

    pthread_detach(pthread_self());

    while (1) {
        sock_accepted = Accept(sock_listen, NULL, NULL);

        client_id = -1;
        /* 無効な接続情報がある(=チャットルームに空きがある)かを調べる． */
        for (i = 0; i < N_client; i++) {
            if (Client[i].state == INVALID) {
                client_id = i;
                break;
            }
        }

        if (client_id == -1) {
            /* 接続情報がすべて有効(=チャットルームが満室)であるとき，接続を遮断する． */
            Send(sock_accepted, full_message, strlen(full_message), 0);
            close(sock_accepted);
            continue;
        }

        Client[client_id].state = LOGIN; /* 接続状態をLOGINへ(まだ有効(VALID)ではない) */

        printf("Client[%d] connected.\n", client_id);

        while (1) {
            /* ユーザーネームを取得する */
            Send(sock_accepted, prompt, strlen(prompt), 0);
            strsize = Recv(sock_accepted, loginname, NAMELENGTH - 1, 0);
            loginname[strsize] = '\0';
            loginname[strcspn(loginname, "\r\n")] = '\0'; /* \rないし\nまでを名前として扱う */

            if (strlen(loginname) == 0) continue; /* 取得した名前が空の場合は再試行する． */
            if (strcmp(loginname, "SERVER") != 0) break; /* `SERVER`という名前は予約済みにしておく */

            Send(sock_accepted, reserved_message, strlen(reserved_message), 0);
        }

        /* 接続情報を有効(VALID)にし，select()に使う値を更新する． */
        Client[client_id].sock = sock_accepted;
        FD_SET(sock_accepted, &mask);
        strncpy(Client[client_id].name, loginname, NAMELENGTH);

        if (sock_accepted > Max_sd) {
            Max_sd = sock_accepted;
        }

        Client[client_id].state = VALID;

        snprintf(send_buf, BUFLEN, "`%s` has entered.", Client[client_id].name);
        broadcast(-1, send_buf);
    }

    return (NULL);
}

/**
 * チャットルームからのログアウト処理を担当する関数です．ログアウトが行われるたびに
 * 誰が退室したかに関するメッセージをbroadcast()します．
 */
void logout(int client_id)
{
    int i;
    int strsize;

    /* select()の対象から外し，接続情報を無効(INVALID)にする */
    FD_CLR(Client[client_id].sock, &mask);
    close(Client[client_id].sock);
    Client[client_id].state = INVALID;
    if (Max_sd == Client[client_id].sock) {
        Max_sd = -1;
        for (i = 0; i < N_client; i++) {
            if (Max_sd < Client[i].sock) {
                Max_sd = Client[i].sock;
            }
        }
    }

    snprintf(send_buf, BUFLEN, "`%s` has left.", Client[client_id].name);
    broadcast(-1, send_buf);
}

/**
 * クライアント情報を格納する構造体の配列を初期化します．
 */
void init_client(int sock_listen, int n_client)
{
    int i;

    N_client = n_client;

    if ((Client = (client_info *) malloc(N_client * sizeof(client_info))) == NULL) {
        exit_errmesg("malloc()");
    }

    for (i = 0; i < N_client; i++) {
        Client[i].state = INVALID;
    }
}

/**
 * サーバーを動作させる関数です．ログイン処理を行うスレッドの起動や，受信したメッセージを
 * 処理してbroadcast()させたりします．クライアントの接続が遮断した場合はログアウトしたと
 * みなし，logout()を呼び出します．
 */
void run_server(int port_number, int num_client)
{
    int sock_listen;

    pthread_t tid;

    int client_id;
    int strsize, copysize;
    int i;
    int cur;

    fd_set readfds;
    struct timeval zerotime, timeout;
    zerotime.tv_sec = 0;
    zerotime.tv_usec = 0;

    sock_listen = init_tcpserver(port_number, 10);
    init_client(sock_listen, num_client);

    /* ログイン処理を行うスレッドを起動 */
    int *tharg[LOGIN_THREAD_NUM];
    for (i = 0; i < LOGIN_THREAD_NUM; i++) {
        if ((tharg[i] = (int *) malloc(sizeof(int))) == NULL) {
            exit_errmesg("malloc()");
        }

        *(tharg[i]) = sock_listen;

        if (pthread_create(&tid, NULL, login_thread, (void *) tharg[i]) != 0) {
            exit_errmesg("pthread_create()");
        }
    }

    FD_ZERO(&mask);

    Max_sd = 0;

    /* クライアント間でメッセージを送受する処理 */
    while (1) {
        readfds = mask;
        timeout = zerotime;
        select(Max_sd + 1, &readfds, NULL, NULL, &timeout);

        for (client_id = 0; client_id < N_client; client_id++) {
            if (Client[client_id].state == INVALID) {
                continue;
            }

            /* 有効な接続情報について受信可能かを調べ，適切に処理する */
            if (FD_ISSET(Client[client_id].sock, &readfds)) {
                strsize = Recv(Client[client_id].sock, recv_buf, BUFLEN - 1, 0);
                if (strsize <= 0) { /* 接続の遮断 */
                    logout(client_id);
                    continue;
                }
                recv_buf[strsize] = 0;

                cur = 0;
                while (cur < strsize) { /* 送信されてきたメッセージを適切に他のクライアントへ送信 */
                    copysize = strcspn(recv_buf + cur, "\r\n\0");
                    strncpy(send_buf, recv_buf + cur, copysize);
                    send_buf[copysize] = 0;
                    broadcast(client_id, send_buf);
                    cur += copysize;
                    cur += strspn(recv_buf + cur, "\r\n\0");
                }
            }
        }
    }
}

/**
 * クライアントを動作させる関数です．標準入力のメッセージをサーバーへ送信し，
 * サーバーから受信したメッセージを標準出力に表示します．
 * サーバーが接続を遮断した場合，プログラムを終了させます．
 */
void run_client(char *servername, int port_number)
{
    int sock;
    char s_buf[BUFLEN], r_buf[BUFLEN];
    int strsize;
    fd_set mask, readfds;

    sock = init_tcpclient(servername, port_number);
    printf("Connected.\n");

    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(sock, &mask);

    while (1) {
        readfds = mask;
        select(sock + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(0, &readfds)) {
            fgets(s_buf, BUFLEN, stdin);
            strsize = strlen(s_buf);
            Send(sock, s_buf, strsize, 0);
        }

        if (FD_ISSET(sock, &readfds)) {
            strsize = Recv(sock, r_buf, BUFLEN - 1, 0);
            if (strsize <= 0) {
                printf("Disconnected.");
                exit(0);
            }
            r_buf[strsize] = '\0';
            printf("%s", r_buf);
            fflush(stdout);
        }
    }
}

/**
 * main関数です．起動オプションの処理を担当し，適切な処理を呼び出します．
 */
int main(int argc, char *argv[])
{
    int port_number = DEFAULT_PORT;
    int num_client = DEFAULT_NCLIENT;
    char servername[SERVER_LEN] = "localhost";
    char mode = DEFAULT_MODE;
    int c;

    opterr = 0;
    while (1) {
        c = getopt(argc, argv, "SCs:p:c:h");
        if (c == -1)
            break;

        switch (c) {
        case 'S': /* サーバモードにする */
            mode = 'S';
            break;

        case 'C': /* クライアントモードにする */
            mode = 'C';
            break;

        case 's': /* サーバ名の指定 */
            snprintf(servername, SERVER_LEN, "%s", optarg);
            break;

        case 'p': /* ポート番号の指定 */
            port_number = atoi(optarg);
            break;

        case 'c': /* クライアントの数 */
            num_client = atoi(optarg);
            break;

        case '?':
            fprintf(stderr, "Unknown option '%c'\n", optopt);
        case 'h':
            fprintf(stderr, "Usage(Server): %s -S -p port_number -c num_client\n", argv[0]);
            fprintf(stderr, "Usage(Client): %s -C -s server_name -p port_number\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    switch (mode) {

    case 'S':
        run_server(port_number, num_client);
        break;
    case 'C':
        run_client(servername, port_number);
        break;
    }

    exit(EXIT_SUCCESS);
}
