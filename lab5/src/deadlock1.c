#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>

// Глобальные переменные (общие для всех потоков)
long long result = 1;       // Сюда собираем итоговый результат
int mod = 1;                // Модуль
pthread_mutex_t mutex; // Мьютекс

// Структура для передачи аргументов в поток
struct ThreadArgs {
    int start;
    int end;
};

// Функция, которую выполняет каждый поток
void* thread_func(void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    long long local_product = 1;

    // 1. Считаем произведение чисел на СВОЁМ участке (без мьютекса)
    for (int i = args->start; i <= args->end; i++) {
        local_product = (local_product * i) % mod;
    }

    // 2. Критическая секция: обновляем общий result
    pthread_mutex_lock(&mutex);
    result = (result * local_product) % mod;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char** argv) {
    int k = 0;
    int pnum = 1;

    // Разбор аргументов командной строки
    static struct option options[] = {
        {"pnum", required_argument, 0, 0},
        {"mod",  required_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;
    
    // Флаг "k:" означает, что -k требует аргумент (двоеточие)
    while ((c = getopt_long(argc, argv, "k:", options, &option_index)) != -1) {
        switch (c) {
            case 'k':
                k = atoi(optarg);
                break;
            case 0:
                if (option_index == 0) pnum = atoi(optarg); // --pnum
                if (option_index == 1) mod = atoi(optarg);  // --mod
                break;
        }
    }

    if (k <= 0 || pnum <= 0 || mod <= 0) {
        printf("Usage: %s -k <num> --pnum <num> --mod <num>\n", argv[0]);
        return 1;
    }

    pthread_t threads[pnum];
    struct ThreadArgs args[pnum];

    // Делим диапазон [1, k] на pnum частей
    int chunk_size = k / pnum;
    int remainder = k % pnum;
    int current = 1;

    // Создаём потоки и распределяем участки работы
    for (int i = 0; i < pnum; i++) {
        args[i].start = current;
        args[i].end = current + chunk_size - 1;
        
        // Распределяем остаток (если k не делится нацело на pnum)
        if (i < remainder) {
            args[i].end++;
        }
        
        current = args[i].end + 1;

        // Запуск потока
        pthread_create(&threads[i], NULL, thread_func, &args[i]);
    }

    // Ждём завершения всех потоков
    for (int i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Result: %lld\n", result);
    return 0;
}