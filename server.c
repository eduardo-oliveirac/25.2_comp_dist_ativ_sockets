/*
    server.c
    Servidor da mini calculadora multi clientes usando select()
    Compilação manual: gcc -Wall -Wextra -02 server.c -o server
    Execução: ./server <port>

    Fluxo da execução:
     1) socket(AF_INEF, SOCK_STEAM, 0)
     2) bind(...)
     3) listen(...)
     4) loop: select(...) -> accept para novos clientes, recv() para mensagens
*/ 


#include <stdio.h>      // operações de i/o
#include <stdlib.h>     //exit, atoi...
#include <string.h>     //textos (memset, strncpy, strnlen, ...)
#include <unistd.h>     //close, read, wirte
#include <errno.h>      //códiggos globais
#include <sys/select.h> //fd_set
#include <sys/types.h>  //tipos básicos para o socket
#include <sys/socket.h> //socket
#include <netinet/in.h> //struct para htons, sockaddr_in
#include <arpa/inet.h>  //inet (ipv4)
#include <signal.h>     // sigint handler
#include <ctype.h>      // isdigit()

// definir o número máx. de clients controlado pelo select
#define MAX_CLIENTS     FD_SETSIZE
#define BUF_SIZE        1024
#define MAX_PEND_CONN   8 //número máximo de conns pendentes 

// opcoes oferecidas pelo server
char *opcoes_server[] = {"QUIT", "ADD", "SUB", "MUL", "DIV"};
int num_opcoes = 5;

int keepRunning = 1; // flag para encerrar programa

static void die(const char *msg);
static int parser(char *buf, int buf_size, float *a, float *b);
static int calculator(char *buf, int buf_size, float *x);
static void sigintHandler(); // executada ao receber SIGINT


int main(int argc, char **argv) {
    // inicia o handler para finalizar ordenadamente ao receber SIGINT
    signal(SIGINT, sigintHandler);
    int port = 5050; // porta padrao

    if (argc > 2) {
        fprintf(stderr, "Use %s <porta>\nEx: %s 5001\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) { // usa porta definida pelo comando de execucao
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Porta inválida!!\n");
            return EXIT_FAILURE;
        }
    }


    // criação do socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { // erro na criacao do socket
        die("socket");
    }

    int yes = 1; // para habilitar o reuso da porta após o fechamento do servidor

    // socket, level (nível de opção), otpname (reusar ou não o endereço), optval, optlen
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        die("setsockopt(SO_REUSEADDR)");
    }


    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET; // IPv4
    addr.sin_port = htons((uint16_t)port); // converte de host byte order para network byte order
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // converte de host byte order para network byte order

    // bind - atribui endereco para socket
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        die("bind");
    }
        
    
    // listen - habilita a porta para aceitar requisicoes de conexao
    if (listen(listen_fd, MAX_PEND_CONN) < 0) { 
        die("listen");
    }
        
    
    printf("\nServidor conectdo e ouvindo em 0.0.0.0: %d ...\n", port);

    // vetor de clientes para guards os FDs
    int clients[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1; //aquele "slot" está livre
    }

    fd_set allset, rset;
    FD_ZERO(&allset);
    FD_SET(listen_fd, &allset);
    int maxfd = listen_fd;
    int max_i = -1;

    char buf[BUF_SIZE];

    for (;;) {
        if (!keepRunning) {
            break;
        }
        rset = allset; // cópia (select modifica o set)
        // select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
        //  - nfds:     1 + maior descritor monitorado (maxfd + 1)
        //  - readfds:  conjunto de FDs para verificação de leitura pronta
        //  - writefds: (não usado aqui) NULL
        //  - exceptfds:(não usado) NULL
        //  - timeout:  (bloqueante) NULL para esperar indefinidamente
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue; // interrompido por sinal
            die("select");
        }

        // Novo cliente chegando?
        if (FD_ISSET(listen_fd, &rset)) {
            struct sockaddr_in cliaddr;
            socklen_t clilen = sizeof(cliaddr);
            // accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
            //  - sockfd: socket em listen
            //  - addr:   (saída) endereço do cliente conectado (pode ser NULL)
            //  - addrlen:(entrada/saída) tamanho do addr; ajustado pelo kernel
            int connfd = accept(listen_fd, (struct sockaddr*)&cliaddr, &clilen);
            if (connfd < 0) { // erro ao aceitar conexao da fila
                perror("accept");
            } else {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
                printf("Novo cliente conectado %s:%d (fd=%d)\n", ip, ntohs(cliaddr.sin_port), connfd);

                // Adiciona na lista de clientes
                int i;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] < 0) {
                        clients[i] = connfd;
                        break;
                    }
                }
                if (i == MAX_CLIENTS) {
                    fprintf(stderr, "Muitos clientes, recusando.\n");
                    close(connfd);
                } else {
                    FD_SET(connfd, &allset);
                    if (connfd > maxfd) maxfd = connfd;
                    if (i > max_i) max_i = i;

                    const char *welcome = "Bem-vindo a mini calculadora!\n";
                    send(connfd, welcome, strlen(welcome), 0);
                }
            }
            if (--nready <= 0) continue; // nada mais pronto
        }

        // Verifica dados vindos dos clientes existentes
        for (int i = 0; i <= max_i; i++) {
            int fd = clients[i];
            if (fd < 0) continue;
            if (FD_ISSET(fd, &rset)) {
                ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
                // recv(int sockfd, void *buf, size_t len, int flags)
                //  - sockfd: socket do qual receber
                //  - buf:    buffer destino
                //  - len:    bytes máximos a ler
                //  - flags:  0 normal (sem flags)
                if (n <= 0) {
                    if (n < 0) perror("recv");
                    printf("[SERVER] Cliente fd=%d desconectou.\n", fd);
                    close(fd);
                    FD_CLR(fd, &allset);
                    clients[i] = -1;
                } else {
                    buf[n] = '\0'; // garantir string
                    // Loga mensagem no servidor
                    printf("\n[SERVER] [REQUISICAO RECEBIDA fd=%d] %s", fd, buf);
                    float x = 0;
                    int status = calculator(buf, sizeof(buf), &x);
                    if (status == 0) {
                        printf("[SERVER] Encerrando conexao com cliente fd=%d.\n", fd);
                        close(fd);
                        FD_CLR(fd, &allset);
                        clients[i] = -1;
                    }
                    else if (status == -1){
                        fprintf(stderr, "[SERVER] ERR EINV entrada_invalida\n");
                        const char *fail = "ERR EINV entrada_invalida\n";
                        send(fd, fail, strlen(fail), 0);
                    }
                    else if (status == -2) {
                        fprintf(stderr, "[SERVER] ERR EZDV divisao_por_zero\n");
                        char *fail = "ERR EZDV divisao_por_zero\n";
                        send(fd, fail, strlen(fail), 0);
                    }
                    else {
                        char message[BUF_SIZE] = "OK "; // string que sera enviada ao cliente
                        char number[30];
                        sprintf(number, "%.6f", x); // armazena o resultado em formato de string
                        strcat(message, number); // adiciona o resultado em message
                        strcat(message, "\n"); // adiciona \n em message
                        send(fd, message, strlen(message), 0); // envia o resultado para o cliente
                    }
                }
                if (--nready <= 0) break; // nenhum FD restante pronto
            }
        }
    }

    close(listen_fd);
    return 0;
}

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// realiza o parsing da mensagem do cliente
static int parser(char *buf, int buf_size, float *a, float *b) {
    char *token = strtok(buf, "\n"); // le primeira palavra da mensagem do cliente
    if (token == NULL) { 
        return -1;
    }
    if (!strncmp(token,"QUIT", buf_size)) { // se for operacao QUIT seguido de \n
        printf("[SERVER] OPERACAO: %s\n", token); // msg log servidor para operacao
        return 0; // retorna codigo de sair
    }
    token = strtok(buf, " ");
    int i;

    for (i = 0; i < num_opcoes; i++) { // se for operacao valida (ADD, SUB, MUL, DIV, QUIT)
        if (!strcmp(token,opcoes_server[i])) {
            printf("[SERVER] OPERACAO: %s (%d)\n", token, i); // msg log servidor para operacao
            break;
        }
    }
    
    if (i == num_opcoes) { // se for operacao invalida
        return -1; // retorna codigo de entrada invalida
    }

    if (i == 0) {
        return i;
    }

    char *ptr;

    // le o primeiro numero
    token = strtok(NULL, " "); 
    if (token == NULL) { 
        return -1;
    }
    ptr = token;
    while (*ptr != '\0') { // verifica se a eh numero valido
        if (!isdigit(*ptr)) {
            return -1;
        }
        ptr++;
    }
    *a = atof(token); // armazena em a
    printf("[SERVER] a = %.6f \n", *a); // msg log servidor do valor de a

    // le o segundo numero
    token = strtok(NULL, " "); 
    if (token == NULL || !isdigit(*token)) { 
        return -1;
    }

    ptr = token;
    while (*ptr != '\0') { // verifica se b eh numero valido
        if (!isdigit(*ptr)) {
            return -1;
        }
        ptr++;
    }

    *b = atof(token); // armazena em b
    printf("[SERVER] b = %.6f \n", *b); // msg log servidor do valor de b

    token = strtok(NULL, " "); // verifica se ha uma terceira entrada
    
    if (token != NULL) { // numero de entradas invalido
        return -1; // retorna codigo de entrada invalida
    }
    return i; // retorna codigo da operacao 
}

// chama o parser e trata a mensagem de acordo
static int calculator(char *buf, int buf_size, float *x) {
    float a, b; // operandos
    int status = parser(buf, buf_size, &a, &b);
    switch (status) {
        case 1:
            *x = a + b;
            break;
        case 2:
            *x = a - b;
            break;
        case 3:
            *x = a * b;
            break;
        case 4:
            if (b == 0) {
                status = -2; // erro divisao por zero
                break;
            }
            *x = a / b;
            break;
        default:
            break;
    }
    if (status > 0) {
        printf("[SERVER] RESULTADO: %.6f \n", *x); // msg log servidor para resultado
    }
    return status;
}

// executada ao receber o sinal SIGINT (CTRL+C)
static void sigintHandler() {
    printf("\n[SERVER] SIGINT (CTRL+C). Encerrando.\n");
    keepRunning = 0;
}