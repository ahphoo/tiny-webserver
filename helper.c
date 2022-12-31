#include "tiny.h"

void rio_readinitb(rio_t* rp, int fd)
{
  rp->rio_fd = fd;
  rp->rio_cnt = 0;
  rp->rio_bufptr = rp->rio_buf;
}


ssize_t rio_read(rio_t* rp, char* usrbuf, size_t n)
{
  int cnt;

  /* Refill if buffer is empty */
  while (rp->rio_cnt <= 0) {
    // Use UNIX read to populate internal buffer
    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));

    /* Interrupted by signal handler return */
    if (rp->rio_cnt < 0 && errno != EINTR) {
      return -1;
    }

    /* EOF */
    if (rp->rio_cnt == 0) {
      return 0;
    }
    
    /* Reset buffer ptr */
    rp->rio_bufptr = rp->rio_buf;
  }

  /* Copy bytes from internal buffer to user buffer */
  cnt = n;
  if (rp->rio_cnt < n) {
    cnt = rp->rio_cnt;
  }
  memcpy(usrbuf, rp->rio_bufptr, cnt);

  /* Adjust buffer ptr and number of unread bytes in the internal buffer */
  rp->rio_bufptr += cnt;
  rp->rio_cnt -= cnt;

  return cnt;
}


ssize_t rio_readlineb(rio_t* rp, char* usrbuf, size_t maxlen)
{
  int n;
  int rc;
  char c;
  char* bufp = usrbuf;

  /* Attempt to read maxlen bytes */
  for (n = 1; n < maxlen; n++) {
    /* Read a byte successfully */
    if ((rc = rio_read(rp, &c, 1)) == 1) {
      *bufp++ = c;

      /* Finished reading a line. Exit loop */
      if (c == '\n') {
        n++;
        break;
      }
    /* EOF */
    } else if (rc == 0) {
      if (n == 1) {
        return 0;  /* No data was read */
      } else {
        break;  /* Some data was read */
      }
    } else {
      return -1;  /* Error */
    }
  }

  /* Null terminate */
  *bufp = 0;
  return n - 1;
}


ssize_t rio_writen(int fd, void* usrbuf, size_t n)
{
  size_t nleft = n;
  ssize_t nwritten;
  char* bufp = usrbuf;

  /* Still bytes left to write */
  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
      if (errno == EINTR)
        nwritten = 0;  /* Interrupted by signal handler */
      else
        return -1;  /* Error */
    }
    nleft -= nwritten;
    bufp += nwritten;
  }

  return n;
}


int open_listenfd(char* port)
{
  struct addrinfo hints;
  struct addrinfo* listp;
  struct addrinfo* p;
  int listenfd;
  int optval = 1;

  /* Get a list of potential server addresses */
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
  getaddrinfo(NULL, port, &hints, &listp);

  /* Walk the list for a socket we can bind to */
  for (p = listp; p; p = p->ai_next) {
    /* Create a socket descriptor */
    if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
      continue;
    }
   
    /* Eliminate "Address already in use" error from bind */
    setsockopt(listenfd,
               SOL_SOCKET,
               SO_REUSEADDR,
               (const void *) &optval,
               sizeof(int));

    /* Bind the descriptor to the address */
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
      break;
    }

    /* Bind failed, try the next socket */
    close(listenfd);
  }

  /* Clean-up */
  freeaddrinfo(listp);

  /* No address could be binded to */
  if (!p) {
    return -1;
  }

  /* Make the socket a listening socket */
  if (listen(listenfd, LISTENQ) < 0) {
    close(listenfd);
    return -1;
  }

  return listenfd;
}
