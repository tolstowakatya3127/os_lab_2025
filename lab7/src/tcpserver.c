#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr // Макрос для сокращения приведения типов

int main(int argc, char *argv[]) {
  if (argc != 3) {  // Проверяем, что передано ровно 2 аргумента (порт и размер буфера)
    printf("Usage: %s <port> <bufsize>\n", argv[0]);
    exit(1);
  }
  
  int port = atoi(argv[1]); // Преобразуем строки в числа
  int bufsize = atoi(argv[2]);
  
  char *buf = malloc(bufsize); // Выделяем память под буфер динамически (размер известен только в runtime)

  const size_t kSize = sizeof(struct sockaddr_in);
  int lfd, cfd, nread; // lfd - listen fd, cfd - client fd
  struct sockaddr_in servaddr, cliaddr;

  if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {// 1. Создаем TCP-сокет (SOCK_STREAM)
    perror("socket");
    exit(1);
  }
  
  memset(&servaddr, 0, kSize);  // 2. Настраиваем адрес сервера
  servaddr.sin_family = AF_INET;             // IPv4
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // Принимать на всех доступных IP
  servaddr.sin_port = htons(port);           // Порт из аргументов (переводим в сетевой порядок байт)

  if (bind(lfd, (SADDR *)&servaddr, kSize) < 0) { // 3. Привязываем сокет к адресу и порту
    perror("bind");
    exit(1);
  }

  if (listen(lfd, 5) < 0) { // 4. Переводим сокет в режим прослушивания (очередь до 5 клиентов)
    perror("listen");
    exit(1);
  }

  while (1) {   // Бесконечный цикл принятия соединений
    unsigned int clilen = kSize;
    
    if ((cfd = accept(lfd, (SADDR *)&cliaddr, &clilen)) < 0) {  // 5. Ждем подключения клиента. Создается новый сокет cfd
      perror("accept");
      exit(1);
    }
    printf("connection established\n");
    
    while ((nread = read(cfd, buf, bufsize)) > 0) {   // 6. Читаем данные от клиента, пока он не закроет соединение
      write(1, buf, nread);   // Выводим полученные данные в консоль (fd 1 - это stdout)
    }

    if (nread == -1) {
      perror("read");
      exit(1);
    }
    close(cfd); // 7. Закрываем сокет конкретного клиента
  }
  free(buf); // Строка недостижима из-за while(1), но для порядка нужна
  return 0;
}