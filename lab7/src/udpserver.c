#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr
#define SLEN sizeof(struct sockaddr_in)

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <port> <bufsize>\n", argv[0]);
    exit(1);
  }
  int port = atoi(argv[1]);
  int bufsize = atoi(argv[2]);
  
  char *mesg = malloc(bufsize + 1);   // Выделяем память (+1 для ноль-терминатора '\0' при выводе как строки)
  char ipadr[16]; // Буфер для хранения IP-адреса клиента в виде строки

  int sockfd, n;
  struct sockaddr_in servaddr, cliaddr;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {  // 1. Создаем UDP-сокет (SOCK_DGRAM)
    perror("socket problem");
    exit(1);
  }

  memset(&servaddr, 0, SLEN);  // 2. Настраиваем адрес сервера
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if (bind(sockfd, (SADDR *)&servaddr, SLEN) < 0) {  // 3. Привязываем сокет к порту
    perror("bind problem");
    exit(1);
  }
  printf("SERVER starts...\n");

  while (1) {
    unsigned int len = SLEN; // Длина адреса клиента (заполнится после recvfrom)
    
    if ((n = recvfrom(sockfd, mesg, bufsize, 0, (SADDR *)&cliaddr, &len)) < 0) {    // 4. Принимаем датаграмму
      perror("recvfrom");
      exit(1);
    }
    mesg[n] = 0; // Добавляем '\0' в конец, чтобы printf не выводил мусор

    printf("REQUEST %s      FROM %s : %d\n", mesg,  // 5. Выводим запрос, IP-адрес и порт отправителя (переводя из сетевого формата)
           inet_ntop(AF_INET, (void *)&cliaddr.sin_addr.s_addr, ipadr, 16),
           ntohs(cliaddr.sin_port));

    if (sendto(sockfd, mesg, n, 0, (SADDR *)&cliaddr, len) < 0) {    // 6. Отправляем те же данные обратно клиенту по адресу cliaddr
      perror("sendto");
      exit(1);
    }
  }
  free(mesg);
  return 0;
}