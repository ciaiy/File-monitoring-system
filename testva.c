#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>

int fun(int a, ...) {
    va_list args;
    va_start(args, a);
    int *j = va_arg(args, int*);
    printf("j = %d\n", j);
    return 0;
}

int main(void) {
    char msg[256] = {0};
    getcwd(msg, 256);
    printf("%s\n", msg);
    return 0;
}