// chat_client.c
// Cliente do "mini-chat" TCP. Usa select() para ler simultaneamente:
// - stdin (teclado) -> envia ao servidor
// - socket -> imprime mensagens vindas do servidor
//
// Compilar: gcc -Wall -Wextra -O2 -o chat_client chat_client.c
// Executar: ./chat_client 127.0.0.1 5000

#include <stdio.h>      // printf, perror, fgets
#include <stdlib.h>     // exit, atoi
#include <string.h>     // memset, strlen
#include <unistd.h>     // close, read, write
#include <errno.h>      // errno
#include <sys/select.h> // fd_set
#include <sys/types.h>  // tipos básicos
#include <sys/socket.h> // socket, connect, send, recv
#include <netinet/in.h> // sockaddr_in, htons
#include <arpa/inet.h>  // inet_pton
#include <signal.h>     // sigint handler

#define BUF_SIZE 1024

static void die(const char *msg);
static void sigintHandler(); // executada ao receber SIGINT
int keepRunning = 1; // flag para encerrar programa

int main(int argc, char **argv) {
    // inicia o handler para finalizar ordenadamente ao receber SIGINT
    signal(SIGINT, sigintHandler);

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <ip-servidor> <porta>\nEx.: %s 127.0.0.1 5050\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        printf("Error: Falha ao registrar handler para SIGINT.\n");
        return -1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Porta inválida.\n");
        return EXIT_FAILURE;
    }

    // 1) socket(AF_INET, SOCK_STREAM, 0)
    //  - AF_INET: IPv4
    //  - SOCK_STREAM: TCP
    //  - 0: protocolo padrão para TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { // erro ao criar socket
        die("socket");
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    // Preenche o endereço do servidor
    servaddr.sin_family = AF_INET;               // família IPv4
    servaddr.sin_port   = htons((uint16_t)port); // porta em ordem de rede
    // inet_pton(int af, const char *src, void *dst)
    //  - af: família do endereço (AF_INET)
    //  - src: string do IP (e.g., "127.0.0.1")
    //  - dst: ponteiro onde será armazenado em binário (servaddr.sin_addr)
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "IP inválido: %s\n", server_ip);
        close(sockfd);
        return EXIT_FAILURE;
    }

    // 2) connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    //  - sockfd: socket criado
    //  - addr:   ponteiro p/ endereço do servidor (IP/porta)
    //  - addrlen:tamanho da struct
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) { // erro ao estabelecer conexao com servidor
        die("connect");
    }

    printf("[CLIENT] Conectado em %s:%d\n", server_ip, port);
    printf("[CLIENT] Digite sua requisicao e pressione ENTER. Digite QUIT ou pressione Ctrl+C para sair.\n");
    printf("[CLIENT] Ex: ADD 1 2\n\n");

    fd_set rset;
    char sendbuf[BUF_SIZE];
    char recvbuf[BUF_SIZE];

    for (;;) {
        if (!keepRunning) {
            break;
        }
        FD_ZERO(&rset);
        FD_SET(STDIN_FILENO, &rset); // monitorar teclado (terminal)
        FD_SET(sockfd, &rset);       // monitorar socket (mensagem servidor)
        int maxfd = (STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd);

        // select espera até haver dados no teclado ou no socket
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue;
            die("select");
        }

        // Entrada do teclado?
        if (FD_ISSET(STDIN_FILENO, &rset)) {
            // fgets lê até '\n' ou EOF
            if (fgets(sendbuf, sizeof(sendbuf), stdin) == NULL) { 
                // EOF (CTRL+D): encerramos a conexão ordenadamente
                printf("\n[CLIENT] EOF de teclado. Encerrando.\n");
                break;
            }
            size_t len = strlen(sendbuf);
            if (len > 0) {
                // send(int sockfd, const void *buf, size_t len, int flags)
                //  - sockfd: nosso socket conectado ao servidor
                //  - buf:    mensagem a enviar
                //  - len:    tamanho em bytes
                //  - flags:  0 para envio "normal"
                ssize_t n = send(sockfd, sendbuf, len, 0);
                if (n < 0) { // erro eo enviar mensagem
                    perror("send");
                    break;
                }
            }
        }

        // Dados vindos do servidor?
        if (FD_ISSET(sockfd, &rset)) {
            // recv retorna 0 quando o servidor fecha a conexão
            ssize_t n = recv(sockfd, recvbuf, sizeof(recvbuf) - 1, 0);
            // recv(int sockfd, void *buf, size_t len, int flags):
            //  - sockfd: socket conectado
            //  - buf:    buffer de recepção
            //  - len:    tamanho máximo
            //  - flags:  0 (sem flags)
            if (n <= 0) {
                if (n < 0) perror("recv");
                printf("[CLIENT] Servidor encerrou a conexão.\n");
                break;
            }
            recvbuf[n] = '\0';
            printf("[SERVER] ");
            printf("%s", recvbuf); // já contém '\n' em geral
            fflush(stdout);
        }
    }

    close(sockfd); // encerra o socket
    return 0;
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// executada ao receber o sinal SIGINT (CTRL+C)
static void sigintHandler() {
    printf("\n[CLIENT] SIGINT (CTRL+C). Encerrando.\n");
    keepRunning = 0;
}