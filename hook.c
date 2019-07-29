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
#include <sys/un.h>

typedef ssize_t (*READ)(int, void *buf, size_t count);
typedef int (*OPEN)(const char *, int, ...);
typedef ssize_t (*WRITE)(int, const void *, size_t);

int sendtoserver(char *info, int sockfd, int type);
ssize_t read_disconnect(int, void *, size_t);
int connectserver();
char *getpathbyfd(int fd, char *realaddr);
char *getpathbypath(char *realpath, const char *pathname);
char *getprocname(int pid);

char *getprocname(int pid)
{
  char path[256] = {0};
  char *procname = malloc(256);
  sprintf(path, "/proc/%d/status", pid);
  FILE *fp = fopen(path, "r");
  fscanf(fp, "Name:	%s", procname);
  return procname;
}

char *getpathbypath(char *realpath, const char *pathname)
{
  if (pathname[0] == '/')
  {
    strcpy(realpath, pathname);
    // int tempsockfd = connectserver();
    // sendtoserver(realpath, tempsockfd, 3);
    return realpath;
  }
  else if (strncmp("./", pathname, 2) == 0)
  {
    getcwd(realpath, 256);
    strcat(&realpath[strlen(realpath)], &pathname[1]);
    // int tempsockfd = connectserver();
    // sendtoserver(realpath, tempsockfd, 4);
    return realpath;
  }
  else
  {
    getcwd(realpath, 256);
    int index = strlen(realpath);
    realpath[index++] = '/';
    strcat(&realpath[index], pathname);
    // int tempsockfd = connectserver();
    // sendtoserver(realpath, tempsockfd, 5);
    return realpath;
  }
}

char *getpathbyfd(int fd, char *realaddr)
{
  char fdaddr[256];
  sprintf(fdaddr, "/proc/%d/fd/%d", getpid(), fd);
  int ret = readlink(fdaddr, realaddr, 256);
  if (ret == -1)
  {
    return NULL;
  }
  else
  {
    return realaddr;
  }
}

int connectserver()
{
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un address;
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, "/home/ciaiy/Desktop/codingspace/fc/server_sock");
  if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
  {
    return -1;
  }
  return sockfd;
}

int sendtoserver(char *info, int sockfd, int type)
{
  char msg[256] = {0};
  char *procname = getprocname(getpid());
  sprintf(&msg[4], "%d %d %s %s", getpid(), type, info, procname);
  int len = strlen(&msg[4]);
  strncpy(msg, (char *)&len, 4);
  int writenum = 0;
  if ((writenum = write(sockfd, msg, len + 4)) < 0)
  {
    close(sockfd);
    perror("write error\n");
  }
  free(procname);
  close(sockfd);
}

ssize_t write(int fd, const void *buf, size_t count)
{
  static void *handle = NULL;
  WRITE old_write = NULL;
  handle = dlopen("libc.so.6", RTLD_LAZY);
  old_write = (WRITE)dlsym(handle, "write");
  char realaddr[256] = {0};
  getpathbyfd(fd, realaddr);
  if (strcmp("/home/ciaiy/Desktop/codingspace/fc/temp", realaddr) == 0)
  {
    int sockfd = connectserver();
    if (sockfd == -1)
    {
      printf("write error!\n");
      return -1;
    }
    else
    {
      sendtoserver((char *)buf, sockfd, 3);
      close(sockfd);
      return old_write(fd, buf, count);
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
  char realpath[256] = {0};
  getpathbypath(realpath, pathname);
  if (strcmp(realpath, "/home/ciaiy/Desktop/codingspace/fc/temp") == 0)
  {

    int sockfd = connectserver();
    if (sockfd == -1)
    {
      return old_open(pathname, flags);
    }
    if (type)
    {
      sendtoserver("create", sockfd, 0);
    }
    else
    {
      sendtoserver(NULL, sockfd, 0);
    }
  }
  return old_open(pathname, flags);

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
  char realaddr[256] = {0};
  getpathbyfd(fd, realaddr);
  if (strcmp("/home/ciaiy/Desktop/codingspace/fc/temp", realaddr) == 0)
  {
    int sockfd = 0;
    if ((sockfd = connectserver()) == -1)
    {
      return read_disconnect(fd, buf, count);
    }
    else
    {
      sendtoserver(NULL, sockfd, 2);
    }
  }
  return old_read(fd, buf, count);
}

ssize_t read_disconnect(int fd, void *buf, size_t count)
{
  static  char msg[14] = "it's a secret";
  static int index;
  int num = 0;
  if(index >= 14) {
    return -1;
  }
  for(int i = 0; i < count; i++) {
    if(i + index > 14) {
      break;
    }else {
      num ++;
      *(char *)(buf + i) = msg[index];
      index++;
    }
  }
  return num;
}