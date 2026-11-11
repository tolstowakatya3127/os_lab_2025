#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr
#define SLEN sizeof(struct sockaddr_in)

int main(int argc, char **argv) {
  if (argc != 4) {
    printf("Usage: %s <IP> <port> <bufsize>\n", argv[0]);
    exit(1);
  }
  int port = atoi(argv[2]);
  int bufsize = atoi(argv[3]);
  char *sendline = malloc(bufsize);
  char *recvline = malloc(bufsize + 1); // +1 для '\0'

  int sockfd, n;
  struct sockaddr_in servaddr;

  memset(&servaddr, 0, sizeof(servaddr));  // 1. Настраиваем адрес сервера
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);

  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) < 0) {  // Преобразуем IP из строки в бинарный вид
    perror("inet_pton problem");
    exit(1);
  }
  
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {  // 2. Создаем UDP-сокет (SOCK_DGRAM)
    perror("socket problem");
    exit(1);
  }

  write(1, "Enter string\n", 13);

  while ((n = read(0, sendline, bufsize)) > 0) {  // 3. Читаем ввод пользователя (fd 0 - stdin)
    if (sendto(sockfd, sendline, n, 0, (SADDR *)&servaddr, SLEN) == -1) {    // 4. Отправляем данные серверу
      perror("sendto problem");
      exit(1);
    }

    if (recvfrom(sockfd, recvline, bufsize, 0, NULL, NULL) == -1) {    // 5. Ждем ответ от сервера (recvfrom)
      perror("recvfrom problem");
      exit(1);
    }

    recvline[n] = 0; // Добавляем ноль-терминатор
    printf("REPLY FROM SERVER= %s\n", recvline);
  }
  
  close(sockfd);  // 6. Закрываем сокет и освобождаем память
  free(sendline);
  free(recvline);
  return 0;
}