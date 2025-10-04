# Atividade: Calculadora Cliente–Servidor com Sockets em C

## Integrantes
- Eduardo Oliveira Carvalho - 10417170
- Lucas Tuon de Matos - 10417987


## Compilação
Para compilar ambos, execute:
```bash
make all 
```

Ou compilar individualmente, executando:
- No terminal do servidor:
```bash
make server
```
- No terminal do cliente:
```bash
make client
```

Para limpar os executáveis criados, use:
```bash
make clean
```

## Execução
No terminal do servidor, executar com:
```bash
./server <port>
```



No terminal do cliente, executar com:
```bash
./client <server_ip> <port>
```


## Protocolo
As requisições do cliente devem ser do formato
```
OP A B\n
```
Onde:
- `OP ∈ {ADD, SUB, MUL, DIV}`
- `A, B` são números reais no formato decimal com ponto (ex.: `2`, `-3.5`, `10.0`).

Existe também a requisição QUIT, que termina a conexão entre cliente e servidor
```
QUIT\n
```

## Exemplos de uso
Requisição → Resposta
```
ADD 100 200\n       ->  OK 300\n
ADD 1t0 200\n       ->  ERR EINV entrada_invalida\n
SUB 1   9\n         ->  OK -10\n
MUL -5  3.5\n       ->  OK -17.5\n
MUL J   K\n         ->  ERR EINV entrada_invalida\n
DIV 5   0\n         ->  ERR EZDV divisao_por_zero\n
DIV 20  4\n         ->  OK 5\n
```

## Decisões de projeto e Limitações conhecidas
- O servidor não possui suporte multi-thread, portanto não é capaz de processar múltiplas requisições simultaneamente 
- O servidor processa apenas operações na forma infixa (OP A B)
- O Makefile foi construído de forma a esperar todos os arquivos (`server.c`, `client.c`, `Makefile`) em um mesmo diretório
- Os possíveis código de erro para uma requisição são:
    - EINV: entrada inválida
    - EZDV: sivisão por zero
