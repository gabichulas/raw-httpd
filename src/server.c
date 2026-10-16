#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT "3490"
#define BACKLOG 5

void sigchld_handler(int s) {
  (void)s; // avoid unused variable warning

  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
  return &(((struct sockaddr_in *)sa)->sin_addr);
}

int main(int argc, char *argv[]) {
  int rv, sockfd, new_fd;
  uint sin_size;
  struct addrinfo hints, *servinfo, *p;
  struct sigaction sa;
  struct sockaddr_in their_addr;
  int yes = 1;
  char s[INET_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // ipv4
  hints.ai_socktype = SOCK_STREAM; // tcp
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo) != 0)) {
    fprintf(stderr, "getaddrinfo: %s \n", gai_strerror(rv));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("server: listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  while (1) {
    sin_size = sizeof their_addr;

    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.sin_family,
              get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

    printf("server got connection from: %s\n", s);

    if (!fork()) {
      close(sockfd);
      if (send(new_fd, "Hola :)", 7, 0) == -1) {
        perror("send");
      }
      close(new_fd);
      exit(0);
    }
    close(new_fd);
  }

  return 0;
}
