#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> // для signal, kill, alarm, SIGALRM, SIGKILL

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

#define MAX_CHILDREN 100 // максимальное количество дочерних процессов

pid_t child_pids[MAX_CHILDREN];
int child_count = 0;
bool timeout_flag = false;

// обработчик сигнала SIGALRM
void alarm_handler(int sig) {
    (void)sig; // подавляем предупреждение о неиспользуемом параметре
    timeout_flag = true;
    printf("\nTimeout! Sending SIGKILL to child processes...\n");
    for (int i = 0; i < child_count; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGKILL);
        }
    }
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = 0; // 0 = без таймаута
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0}, // Новая опция
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) { printf("seed must be a positive number\n"); return 1; }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) { printf("array_size must be a positive number\n"); return 1; }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) { printf("pnum must be a positive number\n"); return 1; }
            break;
          case 3:
            with_files = true;
            break;
          case 4:
            timeout = atoi(optarg);
            if (timeout <= 0) { printf("timeout must be a positive number\n"); return 1; }
            break;
          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;
      case '?':
        break;
      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) { printf("Has at least one no option argument\n"); return 1; }
  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"num\"] [--by_files]\n", argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  int pipes[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) { perror("pipe"); return 1; }
    }
  }

  // Устанавливаем обработчик и таймер, если задан timeout
  if (timeout > 0) {
    signal(SIGALRM, alarm_handler);
    alarm(timeout); // Запускаем таймер
  }

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      active_child_processes += 1;
      if (child_pid > 0 && child_count < MAX_CHILDREN) {
        child_pids[child_count++] = child_pid; // Сохраняем PID
      }
      if (child_pid == 0) {
        // Дочерний процесс
        unsigned int begin = i * (array_size / pnum);
        unsigned int end = (i == pnum - 1) ? array_size : (i + 1) * (array_size / pnum);
        struct MinMax min_max = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[50];
          sprintf(filename, "tmp_%d.txt", i);
          FILE *f = fopen(filename, "w");
          fprintf(f, "%d %d", min_max.min, min_max.max);
          fclose(f);
        } else {
          close(pipes[i][0]);
          write(pipes[i][1], &min_max.min, sizeof(int));
          write(pipes[i][1], &min_max.max, sizeof(int));
          close(pipes[i][1]);
        }
        return 0;
      }
    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  // Неблокирующее ожидание с WNOHANG
  while (active_child_processes > 0) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG); // Неблокирующее ожидание
    if (pid > 0) {
      // Ребёнок завершился
      active_child_processes -= 1;
      if (timeout_flag) {
        printf("Child %d was killed.\n", pid);
      }
    } else if (pid == 0) { // Никто не завершился, но таймаут мог сработать
      if (timeout_flag) break; // Таймаут уже обработан в alarm_handler
      usleep(10000); // Небольшая задержка, чтобы не грузить CPU
    } else {
      perror("waitpid");
      break;
    }
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      char filename[50];
      sprintf(filename, "tmp_%d.txt", i);
      FILE *f = fopen(filename, "r");
      if (f) {
        fscanf(f, "%d %d", &min, &max);
        fclose(f);
        remove(filename);
      }
    } else {
      close(pipes[i][1]);
      read(pipes[i][0], &min, sizeof(int));
      read(pipes[i][0], &max, sizeof(int));
      close(pipes[i][0]);
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);
  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}