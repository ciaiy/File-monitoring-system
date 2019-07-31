#include <stdio.h>
#include <sys/un.h>
#include <strings.h>
#include <sys/socket.h>
#include <stdlib.h>
#include "cJSON.h"
#include <pthread.h>
#include <malloc.h>
#include <readline/readline.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h> 

char socket_path[256];

/*   */
void start_server(void);
void *work(void *arg);
void create_config();
void trim(char *);

void trim(char *str) {
    int index = 0;
    while(str[index++] == ' ');
    index--;
    for(int i = 0; str[index] != '\0'; i++, index++) {
        str[i] = str[index];
        if(str[index] == ' ') {
            str[i] = 0;
            break;
        }
    }
    str[index--] = 0;
}

void create_config() {
    if(access("./hook.so", F_OK) == -1) {
        printf("hook.so 已丢失， 请重新安装\n");
        exit(-1);
    }
    
    cJSON *config_info = cJSON_CreateObject();
    char *target_path = malloc(256);
    char *socket_path = malloc(256);
    if(!target_path || !socket_path) {
        perror("malloc error:");
        exit(-1);
    }

    target_path = readline("请输入需要监控的文件目录(255以内):");
    trim(target_path);  // 去掉前后的空格
    cJSON_AddStringToObject(config_info, "target_path", target_path);
    socket_path = readline("请输入socket存储路径(255以内):");
    trim(socket_path);  // 去掉前后的空格
    cJSON_AddStringToObject(config_info, "socket_path", socket_path);
    
    char *config_buf = cJSON_Print(config_info);
    int config_fd;
    int profile_fd;
    if((profile_fd = open("/etc/profile", O_WRONLY|O_APPEND)) == -1) {
        perror("打开配置文件错误(尝试用sudo命令运行)");
        exit(-1);
    }
    if((config_fd = open("/etc/fc_config.json", O_WRONLY|O_CREAT, 0644)) == -1) {
        perror("创建配置文件错误(尝试用sudo命令运行)");
        exit(-1);
    }
    
    if(write(config_fd, config_buf, strlen(config_buf)) < 0) {
        perror("写入配置错误(尝试用sudo命令运行)");
        close(config_fd);
        system("rm /etc/fc_config.json");
        exit(-1);
    }
    close(config_fd);
    free(config_buf);
    cJSON_Delete(config_info);

    lseek(profile_fd, 0, SEEK_END);
    char profile_buf[256] = {0};
    char temp[256] = {0};
    sprintf(profile_buf, "\nexport LD_PRELOAD=\"%s/hook.so\"", getcwd(temp, 256));
    printf("%s\n", profile_buf);
    if(write(profile_fd, profile_buf, 256) < 0) {
        perror("写入环境变量错误(尝试用sudo命令运行)");
        close(profile_fd);
        system("rm /etc/fc_config.json");
        exit(-1);
    }
    close(profile_fd);
    printf("写入环境变量完成, 请重启系统\n");
    exit(0);
}

void *work(void *arg) {
    int clientfd = *(int *)arg;
    int recvlen = 0;
    read(clientfd, &recvlen, 4);
    char recv_buf[recvlen + 1];
    bzero(recv_buf, recvlen + 1);
    read(clientfd, recv_buf, recvlen);
    printf("%s\n", recv_buf);
    close(clientfd);
    free(arg);
    return NULL;

}

void start_server() {
    int fd = open("/etc/fc_config.json", O_RDONLY);
    char config_buf[512] = {0};
    read(fd, config_buf, 512);
    cJSON *config_info = cJSON_Parse(config_buf);
    cJSON *item =  cJSON_GetObjectItem(config_info, "socket_path");
    if(item == NULL) {
        printf("配置文件内容错误， 请尝试删除并重新安装\n");
        exit(-1);
    }
    strcpy(socket_path, item->valuestring);
    cJSON_Delete(config_info);

    unlink(socket_path);
    struct sockaddr_un addr;
    int sockfd;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("socket error");
    }
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socket_path);
    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind error");
    }
    if(listen(sockfd, 10) == -1) {
        perror("listen error");
    }
    printf("服务器已启动，正在监听...\n");
    while(1) {
        struct sockaddr_un clientaddr;
        int addrlen = sizeof(struct sockaddr_un);
        pthread_t worktid;
        int *clientfd = malloc(sizeof(4));
        *clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &addrlen);
        if(*clientfd == -1) {
            perror("accept error");
        }
        pthread_create(&worktid, NULL, work, (void *)clientfd);
    }
}

int main(void) {
    if(access("/etc/fc_config.json", F_OK|R_OK) == -1) {
        create_config();
    }else {
        start_server();
    }
    return 0;
}

