#include <arpa/inet.h>
#include <bits/types/struct_timeval.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "3490"
#define BACKLOG 5

struct HttpHeader {
  char *key;
  char *value;
};

struct HttpRequest {
  char *method;
  char *path;
  char *version;
  struct HttpHeader h[16];
  size_t size_h;
  char *body;
  size_t body_len;
};

void *get_in_addr(struct sockaddr *sa) {
  return &(((struct sockaddr_in *)sa)->sin_addr);
}

int send_all(int fd, const void *buf, size_t len) {
  const char *ptr = (const char *)buf;

  while (len > 0) {
    ssize_t byt = send(fd, ptr, len, 0);
    if (byt == -1) {
      return -1;
    } else if (byt > 0) {
      len -= byt;
      ptr += byt;
    }
  }
  return 0;
}

void send_response(int fd, int status, char *reason, const char *type,
                   const void *body, size_t len) {
  char hdr[512];

  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %d %s\r\n"
                   "Content-Type: %s\r\n"
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   status, reason, type, len);

  if (n < 0 || n >= (int)sizeof(hdr)) {
    return;
  }

  if (send_all(fd, hdr, n) == 0) {
    send_all(fd, body, len);
  }
}

int check_rnrn(char *delim) {
  if (*(delim + 1) == '\r') {
    if (*(delim + 2) == '\n') {
      return 0;
    }
  }
  return -1;
}

int parse_request(char *buf, size_t len, struct HttpRequest *req) {
  int i = 0;
  if (len <= 0)
    return -1;

  char *p = buf;

  char *sp_1 = strchr(buf, ' ');
  if (sp_1 == NULL)
    return -1;

  *sp_1 = '\0';
  req->method = p;

  p = sp_1 + 1;

  char *sp_2 = strchr(p, ' ');
  if (sp_2 == NULL)
    return -1;

  *sp_2 = '\0';
  req->path = p;

  p = sp_2 + 1;

  char *rn = strstr(p, "\r\n");
  if (rn == NULL)
    return -1;

  *rn = '\0';
  req->version = p;

  p = rn + 1;

  while (1) {
    if (check_rnrn(p) == 0) {
      p = p + 3;
      if (*p != '\0') {
        req->body = p;
        req->body_len = sizeof(*p);
      }
      req->size_h = sizeof(*req->h);
      return 0;
    }

    char *semicolon = strstr(p, ": ");
    if (semicolon == NULL) {
      printf("header key not found");
      return -1;
    }

    *semicolon = '\0';

    struct HttpHeader header;
    header.key = p;

    // printf("%s \n", p);
    p = semicolon + 2;

    char *head_lim = strstr(p, "\r\n");
    if (head_lim == NULL) {
      printf("header value not found");
      return -1;
    }
    printf("%s \n", p);

    *head_lim = '\0';

    header.value = p;
    p = head_lim + 1;
    req->h[i] = header;
    i++;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int rv, sockfd, new_fd;
  socklen_t sin_size;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_in their_addr;
  int yes = 1;
  char s[INET_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // ipv4
  hints.ai_socktype = SOCK_STREAM; // tcp
  hints.ai_flags = AI_PASSIVE;

  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 0;

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
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

  while (1) {
    sin_size = sizeof their_addr;

    char buf[2048];

    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    if ((setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)) {
      perror("setsockopt");
      close(new_fd);
      continue;
    }

    inet_ntop(their_addr.sin_family,
              get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

    printf("server got connection from: %s\n", s);
    ssize_t bytes_recv = recv(new_fd, buf, sizeof(buf) - 1, 0);

    if (bytes_recv <= 0) {
      if (bytes_recv == 0) {
        printf("client disconnected \n");
      } else {
        perror("recv");
      }
      close(new_fd);
      continue;
    }

    buf[bytes_recv] = '\0';

    struct HttpRequest req;

    if (parse_request(buf, sizeof(buf), &req) == -1) {
      perror("parse_request");
      close(new_fd);
      continue;
    }

    printf("%s %s %s %s %c\n", req.method, req.path, req.version, req.body,
           (int)req.size_h);

    const char *html_body = "<html><body><h1>Hola :)</h1></body></html>";

    send_response(new_fd, 200, "OK", "text/html", html_body, strlen(html_body));

    close(new_fd);
  }

  return 0;
}
