#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include "common.h"

struct Server {
  char ip[255];
  int port;
};

// Структура для передачи данных в поток
struct ClientTask {
    struct Server server;
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
    uint64_t result;
};


bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  errno = 0; // Сбрасываем errno перед вызовом
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }
  if (errno != 0) return false;
  *val = i;
  return true;
}

// Функция, которая выполняется в отдельном потоке для каждого сервера
void *ServerWorker(void *arg) {
  struct ClientTask *task = (struct ClientTask *)arg;
  
  struct hostent *hostname = gethostbyname(task->server.ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", task->server.ip);
    return NULL;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(task->server.port);
  server_addr.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", task->server.ip, task->server.port);
    close(sck);
    return NULL;
  }

  char payload[sizeof(uint64_t) * 3];
  memcpy(payload, &task->begin, sizeof(uint64_t));
  memcpy(payload + sizeof(uint64_t), &task->end, sizeof(uint64_t));
  memcpy(payload + 2 * sizeof(uint64_t), &task->mod, sizeof(uint64_t));

  if (send(sck, payload, sizeof(payload), 0) < 0) {
    fprintf(stderr, "Send failed\n");
    close(sck);
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (recv(sck, response, sizeof(response), 0) < 0) {
    fprintf(stderr, "Receive failed\n");
    close(sck);
    return NULL;
  }

  memcpy(&task->result, response, sizeof(uint64_t));
  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = 0;
  uint64_t mod = 0;
  char servers_path[255] = {'\0'};

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        ConvertStringToUI64(optarg, &k);
        break;
      case 1:
        ConvertStringToUI64(optarg, &mod);
        break;
      case 2:
        memcpy(servers_path, optarg, strlen(optarg));
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;
    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (k == 0 || mod == 0 || !strlen(servers_path)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
    return 1;
  }

  // Чтение файла с серверами
  FILE *f = fopen(servers_path, "r");
  if (!f) {
      perror("Failed to open servers file");
      return 1;
  }

  struct Server *servers = malloc(sizeof(struct Server) * 100);
  int servers_num = 0;
  char line[256];

  while (fgets(line, sizeof(line), f)) {
      line[strcspn(line, "\r\n")] = 0; // Удаляем \n и \r
      if (strlen(line) == 0) continue;

      char *colon = strchr(line, ':');
      if (!colon) continue;

      *colon = '\0';
      strcpy(servers[servers_num].ip, line);
      servers[servers_num].port = atoi(colon + 1);
      servers_num++;
  }
  fclose(f);

  if (servers_num == 0) {
      fprintf(stderr, "No servers found in file\n");
      free(servers);
      return 1;
  }

  // Распараллеливание между серверами
  pthread_t threads[servers_num];
  struct ClientTask tasks[servers_num];

  uint64_t chunk = k / servers_num;
  uint64_t remainder = k % servers_num;
  uint64_t current = 1;

  for (int i = 0; i < servers_num; i++) {
      tasks[i].server = servers[i];
      tasks[i].begin = current;
      tasks[i].end = current + chunk - 1;
      
      if (i < remainder) {
          tasks[i].end++;
      }
      
      tasks[i].mod = mod;
      tasks[i].result = 1;
      current = tasks[i].end + 1;

      if (pthread_create(&threads[i], NULL, ServerWorker, &tasks[i])) {
          fprintf(stderr, "Error: pthread_create failed!\n");
          free(servers);
          return 1;
      } 
  }

  // Сбор результатов
  uint64_t total_answer = 1;
  for (int i = 0; i < servers_num; i++) {
      pthread_join(threads[i], NULL);
      total_answer = MultModulo(total_answer, tasks[i].result, mod);
  }

  printf("Total answer: %llu\n", (unsigned long long)total_answer);

  free(servers);
  return 0;
}