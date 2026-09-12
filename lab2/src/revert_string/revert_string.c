#include "revert_string.h"
#include <string.h> // Необходимо для strlen

void RevertString(char *str)
{
    if (str == NULL || *str == '\0') {	// Если строка пустая или NULL, ничего не делаем
        return;
    }
    char *start = str;	// Указатель на начало строки
    char *end = str + strlen(str) - 1;	// Указатель на конец строки (последний символ перед '\0')

    while (start < end) {	// Меняем символы местами, пока указатели не встретятся
        char temp = *start;	// Обмен значениями по указателям
        *start = *end;
        *end = temp;
        start++;	// Сдвигаем указатели навстречу друг другу
        end--;
    }
}