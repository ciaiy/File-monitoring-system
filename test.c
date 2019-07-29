#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



int main(int argc, char **argv) {
    int fd = open("./temp" , O_RDWR);
    char buf[100] = {0};
    printf("my test pid = %d\n", getpid());
    read(fd, (void *)buf, (size_t)20);
    printf("has read : %s\n", buf);
    char writebuf[10] = "123";
    int num = write(fd, writebuf, 3);
    printf("write : %d\n", num);
    sleep(100);
    return 0;
}
