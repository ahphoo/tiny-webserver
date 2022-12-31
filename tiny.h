/*
 * Contains the definitions and methods for the tiny webserver
 */
#ifndef __TINY_H__
#define __TINY_H__

#define MAXLINE 8192 /* The max text length */
#define MAXBUF 8192 /* The max I/O buffer size*/
#define RIO_BUFSIZE 8192 /* Same as MAXBUF buf for the I/O implementation */
#define LISTENQ 1024 /* The max number of outstanding connection requests */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netdb.h>

extern char** environ;

typedef struct {
  int rio_fd;
  int rio_cnt;
  char* rio_bufptr;
  char rio_buf[RIO_BUFSIZE];
} rio_t;

/* Main methods */
void doit(int fd);
void read_requesthdrs(rio_t* rp);
int parse_uri(char* uri, char* filename, char* cgiargs);
void serve_static(int fd, char* filename, int filesize);
void get_filetype(char* filename, char* filetype);
void serve_dynamic(int fd, char* cause, char* errnum);
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);

/* Helper methods */
void rio_readinitb(rio_t* rp, int fd);
ssize_t rio_read(rio_t* rp, char* usrbuf, size_t n);
ssize_t rio_readlineb(rio_t* rp, char* usrbuf, size_t maxlen);
ssize_t rio_writen(int fd, void* usrbuf, size_t n);
int open_listenfd(char* port);

#endif
