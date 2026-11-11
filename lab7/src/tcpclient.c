#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr
#define SIZE sizeof(struct sockaddr_in)

int main(int argc, char *argv[]) {
  if (argc != 4) {  // Ожидаем: IP, порт, размер буфера
    printf("Usage: %s <IP> <port> <bufsize>\n", argv[0]);
    exit(1);
  }
  int port = atoi(argv[2]);
  int bufsize = atoi(argv[3]);
  char *buf = malloc(bufsize); // Динамический буфер

  int fd, nread;
  struct sockaddr_in servaddr;
  
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // 1. Создаем TCP-сокет
    perror("socket creating");
    exit(1);
  }

  memset(&servaddr, 0, SIZE);   // 2. Настраиваем адрес сервера
  servaddr.sin_family = AF_INET;
  
  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) { // Преобразуем IP-адрес из строки в бинарный формат
    perror("bad address");
    exit(1);
  }
  servaddr.sin_port = htons(port); // Устанавливаем порт

  if (connect(fd, (SADDR *)&servaddr, SIZE) < 0) {  // 3. Устанавливаем соединение с сервером (TCP handshake)
    perror("connect");
    exit(1);
  }

  write(1, "Input message to send\n", 22); // Приглашение ко вводу
  
  while ((nread = read(0, buf, bufsize)) > 0) {  // 4. Читаем ввод пользователя с клавиатуры (fd 0 - это stdin)
    if (write(fd, buf, nread) < 0) {             // 5. Отправляем прочитанные данные серверу
      perror("write");
      exit(1);
    }
  }
  
  close(fd);  // 6. Закрываем соединение
  free(buf);
  exit(0);
}