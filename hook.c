#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "cJSON.h"
#include <sys/un.h>
#include <errno.h>

#define T_OPEN  0
#define T_READ  1
#define T_WRITE 2

extern int errno;
static cJSON *info;

char *type_info[3] = {"open", "read", "write"};
char *target_path;
char *socket_path;

typedef ssize_t (*READ)(int, void *buf, size_t count);
typedef int (*OPEN)(const char *, int, ...);
typedef ssize_t (*WRITE)(int, const void *, size_t);

/* 向监控程序发送数据 */
int sendtoserver(char *info, int sockfd, int type);

/* 当无连接时，读取的虚拟目标文件 */
ssize_t read_disconnect(int, void *, size_t);

/* 连接监控程序 */
int connectserver();

/* 通过fd获取完整路径 */
char *getpathbyfd(int fd, char *real_path);

/* 通过路径获取完整路径 */
char *getpathbypath(char *real_path, const char *pathname);

/* 获取进程名称 */
char *getprocname(int pid);

/* 初始化配置信息 */
int init_config(void);

int init_config(void)
{
  // 如果已经存在配置信息， 则直接返回
  if (target_path != NULL && socket_path != NULL)
  {
    return 1;
  }
  // 判断是否存在配置文件
  if (access("/etc/fc_config.json", F_OK & R_OK) != -1)
  {
    int fd = open("/etc/fc_config.json", O_RDONLY);
    int readnum = 0;
    char *buf;
    if ((buf = malloc(512)) == NULL)
    {
      return 0; // 出错处理 : malloc 出错
    }
    bzero(buf, 512);
    int ret_value = read(fd, buf, 512);
    if (ret_value != -1 && (target_path == NULL || socket_path == NULL))
    {
      cJSON *info = cJSON_Parse(buf);

      // 获取target_path信息
      cJSON *item = cJSON_GetObjectItem(info, "target_path");
      if (item != NULL)
      {
        target_path = malloc(256);
        bzero(target_path, 256);
        strcpy(target_path, item->valuestring);
      }
      else
      {
        return 0; // 出错处理 : 无法得到target_path
      }

      // 获取socket_path信息
      item = cJSON_GetObjectItem(info, "socket_path");
      if (item != NULL)
      {
        socket_path = malloc(256);
        bzero(socket_path, 256);
        strcpy(socket_path, item->valuestring);
        cJSON_Delete(item); // 删除info以防内存泄漏
      }
      else
      {
        return 0; // 出错处理 : 无法得到socket_path
      }
      return 1; // 获取完配置信息
    }
  }

  // 出错处理 : 无配置文件或读取配置文件错误
  return 0;
}

char *getprocname(int pid)
{
  char path[256] = {0};
  char *procname = NULL;
  procname = malloc(256);
  sprintf(path, "/proc/%d/status", pid);
  FILE *fp = fopen(path, "r");
  if (fp == NULL || procname == NULL)
  {
    return "unknown process";
  }
  fscanf(fp, "Name:	%s", procname);
  return procname;
}

char *getpathbypath(char *real_path, const char *pathname)
{
  if (pathname[0] == '/') // 绝对路径
  {
    strcpy(real_path, pathname);
    return real_path;
  }
  else if (strncmp("./", pathname, 2) == 0) // 相对路径
  {
    getcwd(real_path, 256);
    strcat(&real_path[strlen(real_path)], &pathname[1]);
    return real_path;
  }
  else // 仅文件名
  {
    getcwd(real_path, 256);
    int index = strlen(real_path);
    real_path[index++] = '/';
    strcat(&real_path[index], pathname);
    return real_path;
  }
}

char *getpathbyfd(int fd, char *real_path)
{
  char fd_path[256];
  sprintf(fd_path, "/proc/%d/fd/%d", getpid(), fd);
  int ret = readlink(fd_path, real_path, 256);
  if (ret == -1)
  {
    return NULL; // 出错处理 : 读取链接文件错误
  }
  else
  {
    return real_path;
  }
}

int connectserver()
{
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un address;
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, socket_path);
  if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
  {
    return -1; // 出错处理 : 无法连接文件监控程序
  }
  return sockfd;
}

int sendtoserver(char *info, int sockfd, int type)
{
  char *send_buf;
  char *procname = getprocname(getpid());
  cJSON *send_info = cJSON_CreateObject();
  cJSON_AddStringToObject(send_info, "procname", procname);
  cJSON_AddNumberToObject(send_info, "pid", getpid());
  cJSON_AddStringToObject(send_info, "type", type_info[type]);
  cJSON_AddStringToObject(send_info, "info", info);
  char *temp = cJSON_Print(send_info);
  send_buf = calloc(1, strlen(temp) + 5);
  if(send_buf == NULL) {
    return -1;
  }
  strcpy(send_buf + 4, temp);
  int len = strlen(&send_buf[4]);
  strncpy(send_buf, (char *)&len, 4);
  int writenum = 0;
  int sum = 0;
  while ((writenum = write(sockfd, send_buf + sum, len + 4 - sum)))
  {
    sum += writenum;
    if (writenum == -1)
    {
      return -1;

    }
  }
  free(procname);
  free(temp);
  free(send_buf);
  cJSON_Delete(send_info);
  close(sockfd);
  return sum;
}

ssize_t write(int fd, const void *buf, size_t count)
{
  static void *handle = NULL;
  WRITE old_write = NULL;
  handle = dlopen("libc.so.6", RTLD_LAZY);
  old_write = (WRITE)dlsym(handle, "write");
  char real_path[256] = {0};
  getpathbyfd(fd, real_path);
  if (strcmp(real_path, "/etc/fc_config.json"))
  {
    if (init_config() && !strcmp(target_path, real_path))
    {
      int sockfd = connectserver();
      if (sockfd == -1)
      {
        errno = ECONNREFUSED;
        return -1;
      }
      else
      {
        sendtoserver((char *)buf, sockfd, T_WRITE);
        close(sockfd);
        return old_write(fd, buf, count);
      }
    }
  }
  return old_write(fd, buf, count);
}

int open(const char *pathname, int flags, ...)
{
  static void *handle = NULL;
  OPEN old_open = NULL;
  int type = 0;
  mode_t mode = 0;

  handle = dlopen("libc.so.6", RTLD_LAZY);
  old_open = (OPEN)dlsym(handle, "open");
  va_list args;
  if (flags & O_CREAT)
  {
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    type = 1;
  }
  char real_path[256] = {0};
  getpathbypath(real_path, pathname);
  if (strcmp("/etc/fc_config.json", real_path))
  {
    if (init_config() && !strcmp(real_path, target_path))
    {

      int sockfd = connectserver();
      if (sockfd == -1)
      {
        return old_open(pathname, flags);
      }
      if (type)
      {
        sendtoserver("create", sockfd, T_OPEN);
      }
      else
      {
        sendtoserver(NULL, sockfd, T_OPEN);
      }
    }
  }
  if (type)
  {
    return old_open(pathname, flags, mode);
  }
  else
  {
    return old_open(pathname, flags);
  }
}

ssize_t read(int fd, void *buf, size_t count)
{
  static void *handle = NULL;
  READ old_read = NULL;
  int flag = 0;
  handle = dlopen("libc.so.6", RTLD_LAZY);
  old_read = (READ)dlsym(handle, "read");
  char real_path[256] = {0};
  getpathbyfd(fd, real_path);
  if (strcmp("/etc/fc_config.json", real_path))
  {
    if (init_config() && !strcmp(target_path, real_path))
    {
      int sockfd = 0;
      if ((sockfd = connectserver()) == -1)
      {
        return read_disconnect(fd, buf, count);
      }
      else
      {
        sendtoserver(NULL, sockfd, T_READ);
      }
    }
  }
  return old_read(fd, buf, count);
}

ssize_t read_disconnect(int fd, void *buf, size_t count)
{
  errno = ECONNREFUSED;
  static char msg[14] = "it's a secret";
  static int index;
  int num = 0;
  if (index >= 14)
  {
    return -1;
  }
  for (int i = 0; i < count; i++)
  {
    if (i + index > 14)
    {
      break;
    }
    else
    {
      num++;
      *(char *)(buf + i) = msg[index];
      index++;
    }
  }
  lseek(fd, num, SEEK_CUR);
  return num;
}
