#include <stdio.h>
#include <sys/un.h>
#include <strings.h>
#include <sys/socket.h>
#include <pthread.h>
#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>

void *work(void *arg);

void *work(void *arg) {
    int clientfd = *(int *)arg;
    int recvlen = 0;
    read(clientfd, &recvlen, 4);
    char recvmsg[recvlen + 1];
    bzero(recvmsg, recvlen + 1);
    read(clientfd, recvmsg, recvlen);
    printf("**** %s\n", recvmsg);
    close(clientfd);
    free(arg);
    return NULL;

}


int main(void) {
    unlink("server_sock");
    struct sockaddr_un addr;
    int sockfd;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("socket error");
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "./server_sock");
    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind error");
    }
    if(listen(sockfd, 10) == -1) {
        perror("listen error");
    }
    int index = 0;
    while(1) {
        struct sockaddr_un clientaddr;
        int addrlen = sizeof(struct sockaddr_un);
        pthread_t worktid;
        int *clientfd = malloc(sizeof(4));
        *clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &addrlen);
        if(*clientfd == -1) {
            perror("accept error");
        }
        printf("no.%d sockfd = %d\n", index++, *clientfd);
        pthread_create(&worktid, NULL, work, (void *)clientfd);
    }
}