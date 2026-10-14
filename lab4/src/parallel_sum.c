#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <getopt.h>
#include <utils.h>

#include <pthread.h>

#include "sum.h" // Подключаем заголовок с Sum и SumArgs

// Функция потока (остаётся в parallel_sum.c, так как она специфична для потока)
void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  return (void *)(size_t)Sum(sum_args);
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  // Разбор аргументов командной строки
  while (1) {
    static struct option options[] = {
      {"threads_num", required_argument, 0, 0},
      {"array_size", required_argument, 0, 0},
      {"seed", required_argument, 0, 0},
      {0, 0, 0, 0}
    };

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0: threads_num = atoi(optarg); break;
          case 1: array_size = atoi(optarg); break;
          case 2: seed = atoi(optarg); break;
        }
        break;
      case '?':
        break;
      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (threads_num == 0 || array_size == 0 || seed == 0) {
    printf("Usage: %s --threads_num \"num\" --array_size \"num\" --seed \"num\"\n", argv[0]);
    return 1;
  }

  // Генерация массива (вне замера времени)
  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);

  pthread_t threads[threads_num];
  struct SumArgs args[threads_num];

  // Замер времени начинается здесь
  struct timeval start_time, finish_time;
  gettimeofday(&start_time, NULL);

  // Создание потоков
  for (uint32_t i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = i * (array_size / threads_num);
    args[i].end = (i == threads_num - 1) ? array_size : (i + 1) * (array_size / threads_num);

    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  // Ожидание завершения потоков и сбор результатов
  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    int sum = 0;
    pthread_join(threads[i], (void **)&sum);
    total_sum += sum;
  }

  gettimeofday(&finish_time, NULL);

  // Вычисление времени
  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  printf("Total: %d\n", total_sum);
  printf("Elapsed time: %fms\n", elapsed_time);
  return 0;
}  