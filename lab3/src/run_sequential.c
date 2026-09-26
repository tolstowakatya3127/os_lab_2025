#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        return 1;
    } 
    else if (pid == 0) {
        // Дочерний процесс: заменяем его на sequential_min_max
        // Передаём аргументы: имя программы, seed, array_size
        execl("./sequential_min_max", "sequential_min_max", "42", "1000000", NULL);
        
        // Если execl вернулся, значит произошла ошибка
        perror("execl failed");
        return 1;
    } 
    else {
        // Родительский процесс: ждём завершения дочернего
        int status;
        waitpid(pid, &status, 0);
        printf("Дочерний процесс завершился с кодом: %d\n", WEXITSTATUS(status));
    }

    return 0;
}