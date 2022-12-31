#include "tiny.h"

void serve_dynamic(int fd, char* filename, char* cgiargs)
{
  char buf[MAXLINE];
  char* emptylist[] = { NULL };

  /* Send response headers */
  snprintf(buf, MAXLINE, "HTTP/1.0 200 OK\r\n" );
  rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "Server: Tiny Web Server\r\n");
  rio_writen(fd, buf, strlen(buf));

  if (fork() == 0) {
    /* TODO: Set other CGI environment variables here */
    setenv("QUERY_STRING", cgiargs, 1);
    dup2(fd, STDOUT_FILENO);
    execve(filename, emptylist, environ);
  }

  wait(NULL);  /* Wait until parent reaps the child process */
}

void get_filetype(char* filename, char* filetype)
{
  if (strstr(filename, ".html"))
    strncpy(filetype, "text/html", MAXLINE);
  else if (strstr(filename, ".gif"))
    strncpy(filetype, "image/gif", MAXLINE);
  else if (strstr(filename, ".png"))
    strncpy(filetype, "image/png", MAXLINE);
  else if (strstr(filename, ".jpg"))
    strncpy(filetype, "image/jpg", MAXLINE);
  else
    strncpy(filetype, "text/plain", MAXLINE);
}


void serve_static(int fd, char* filename, int filesize)
{
  int srcfd;
  char* srcp;
  char filetype[MAXLINE];
  char buf[MAXLINE];

  /* Send response headers */
  get_filetype(filename, filetype);
  snprintf(buf, MAXLINE, "HTTP/1.0 200 OK\r\n");
  rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);
  snprintf(buf, MAXLINE, "Server: Tiny Web Server\r\n");
  rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);
  snprintf(buf, MAXLINE, "Connection: close\r\n");
  rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);
  snprintf(buf, MAXLINE, "Content-length: %d\r\n", filesize);
  rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);
  /* Hacky fix for warning */
  snprintf(buf, MAXLINE + 17, "Content-type: %s\r\n", filetype);
  rio_writen(fd, buf, strlen(buf));
  printf("%s\n", buf);

  /* Send response body */
  srcfd = open(filename, O_RDONLY, 0);
  srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  close(srcfd);
  rio_writen(fd, srcp, filesize);
  munmap(srcp, filesize);
}


int parse_uri(char* uri, char* filename, char* cgiargs)
{
  char* ptr;

  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);

    if (uri[strlen(uri) - 1] == '/') {
      strcat(filename, "home.html");
    }

    return 1;
  }
  else {
    ptr = index(uri, '?');

    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else {
      strcpy(cgiargs, "");
    }

    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  rio_readlineb(rp, buf, MAXLINE);
  while (strncmp(buf, "\r\n", MAXLINE)) {
    rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }

  return;
}

/* TODO: Find better way to format HTML */
void clienterror(int fd, char* cause, char* errnum, 
                 char* shortmsg, char* longmsg)
{
  char buf[MAXLINE];

  /* Print the HTTP response headers */
  snprintf(buf, MAXLINE, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "Content-type: text/html\r\n\r\n");
  rio_writen(fd, buf, strlen(buf));

  /* Print the HTTP response body */
  snprintf(buf, MAXLINE, "<html><title>Tiny Error</title>");
  rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "<body bgcolor=""ffffff"">\r\n");
  rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "%s, %s\r\n", errnum, shortmsg);
  rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "<p>%s : %s\r\n", longmsg, cause);
  rio_writen(fd, buf, strlen(buf));
  snprintf(buf, MAXLINE, "<hr><em>The Tiny Web server</em>\r\n");
  rio_writen(fd, buf, strlen(buf));
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE];
  char method[MAXLINE];
  char uri[MAXLINE];
  char version[MAXLINE];
  char filename[MAXLINE];
  char cgiargs[MAXLINE];
  rio_t rio;

  rio_readinitb(&rio, fd);
  rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);

  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }

  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
        return;
    }
    serve_static(fd, filename, sbuf.st_size);
  } 
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't run the CGI program");
        return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

int main(int argc, char* argv[])
{
  int listenfd;
  int connfd;
  char hostname[MAXLINE];
  char port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clientlen);
    getnameinfo((struct sockaddr*) &clientaddr, 
                clientlen,
                hostname,
                MAXLINE,
                port,
                MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    close(connfd);
  }
}
