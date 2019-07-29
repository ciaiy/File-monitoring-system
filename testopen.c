#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(void) {
    int fd =  open("./temp", O_RDONLY, 0777);
    return 0;
}