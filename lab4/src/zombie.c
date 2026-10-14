#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t child_pid = fork();

    if (child_pid == -1) {
        perror("fork failed");
        return 1;
    } 
    else if (child_pid == 0) {
        // Дочерний процесс: немедленно завершается, становясь зомби
        printf("Child process (PID: %d) exiting...\n", getpid());
        exit(0);
    } 
    else {
        // Родительский процесс: спит, не вызывая wait()
        printf("Parent process (PID: %d) is sleeping. Child PID: %d\n", getpid(), child_pid);
        printf("Check for zombie with: ps -eo pid,ppid,stat,comm | grep zombie\n");
        sleep(60); // Спим 60 секунд, чтобы можно было увидеть зомби
        
        // Только после пробуждения родитель "пожинает" зомби
        printf("Parent is now reaping child...\n");
        wait(NULL);
        printf("Zombie has been reaped. Exiting.\n");
    }

    return 0;
}