#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

// Два общих ресурса (мьютекса)
pthread_mutex_t resource1;
pthread_mutex_t resource2;

// Поток 1: сначала resource1, затем resource2
void* thread1_func(void* arg) {
    printf("[Thread 1] Захватываю resource1...\n");
    pthread_mutex_lock(&resource1);
    
    usleep(100000); // Даем потоку 2 время захватить resource2
    
    // Пытаемся захватить resource2 без блокировки
    while (pthread_mutex_trylock(&resource2) != 0) {
        printf("[Thread 1] resource2 занят. Отпускаю resource1 и жду...\n");
        
        // ОТКАТ: освобождаем первый ресурс, чтобы дать другому потоку шанс
        pthread_mutex_unlock(&resource1); 
        
        usleep(50000); // Ждем 50 мс (даем потоку 2 завершить работу)
        
        // Пытаемся снова захватить resource1
        pthread_mutex_lock(&resource1); 
    }
    
    // Сюда попадаем, только если успешно захватили оба
    printf("[Thread 1] Успешно захватил оба ресурса!\n");
    pthread_mutex_unlock(&resource2);
    pthread_mutex_unlock(&resource1);
    return NULL;
}

// Поток 2: сначала resource2, затем resource1
void* thread2_func(void* arg) {
    printf("[Thread 2] Захватываю resource2...\n");
    pthread_mutex_lock(&resource2);
    
    usleep(100000); // Даем потоку 1 время захватить resource1
    
    // Пытаемся захватить resource1 без блокировки
    while (pthread_mutex_trylock(&resource1) != 0) {
        printf("[Thread 2] resource1 занят. Отпускаю resource2 и жду...\n");
        
        // ОТКАТ
        pthread_mutex_unlock(&resource2); 
        
        usleep(50000);
        
        pthread_mutex_lock(&resource2); 
    }
    
    // Сюда попадаем, только если успешно захватили оба
    printf("[Thread 2] Успешно захватил оба ресурса!\n");
    pthread_mutex_unlock(&resource1);
    pthread_mutex_unlock(&resource2);
    return NULL;
}

int main() {
    pthread_t t1, t2;

    printf("Демонстрация решения Deadlock через trylock\n");
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Программа успешно завершена!\n");
    return 0;
}