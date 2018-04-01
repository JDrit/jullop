#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

#define PORT 8080
#define QUEUE_LENGTH 10
#define MAX_EVENTS 10

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, ">>> "); fprintf(stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

void set_sock_opt(int sock, int level, int opt_name) {
  int opt = 1;
  if (setsockopt(sock, level, opt_name, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
}

int create_socket() {
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (sock == -1) {
    perror("getting socket");
    exit(EXIT_FAILURE);
  }

  // allows reusing of the same address and port
  set_sock_opt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT);
  // all writes should send packets right away
  set_sock_opt(sock, SOL_TCP, TCP_NODELAY);

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(sock, (struct sockaddr*) &address, sizeof(address)) == -1) {
    perror("bind failure");
    exit(EXIT_FAILURE);
  }

  DEBUG_PRINT("socket bind complete\n");

  if (listen(sock, QUEUE_LENGTH) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  DEBUG_PRINT("listening on %d\n", PORT);
  
  return sock;
}

int main(int argc, char* argv[]) {
  DEBUG_PRINT("starting up...\n");

  struct epoll_event event;
  struct epoll_event *events;

  int sock_fd = create_socket();
  int epoll_fd = epoll_create(1);
  if (epoll_fd == -1) {
    perror("epoll");
    exit(EXIT_FAILURE);
  }

  event.data.fd = epoll_fd;
  event.events = EPOLLIN | EPOLLET;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
    perror("epoll_ctl");
    exit(EXIT_FAILURE);
  }

  int events = calloc(MAX_EVENTS, sizeof(event));
  int ready_amount = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
  int fd = accept(sock_fd, NULL, 0);



  printf("%d\n", fd);
  
  
    
}
