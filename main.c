#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

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

#define CHECK(x, msg) ({ \
  int ret_val = x; \
  if (ret_val == -1) { \
    fprintf(stderr, ">>> Error on line %d of file %s\n", __LINE__, __FILE__); \
    perror(msg); \
    exit(EXIT_FAILURE); \
  } \
  ret_val; \
})

void set_sock_opt(int sock, int level, int opt_name) {
  int opt = 1;
  CHECK(setsockopt(sock, level, opt_name, &opt, sizeof(opt)), "setsockopt");  
}

int create_socket() {
  int opt = 1;
  int sock = CHECK(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0), "getting socket");

  // allows reusing of the same address and port
  CHECK(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &opt, sizeof(opt)), "sock opt");
  // all writes should send packets right away
  CHECK(setsockopt(sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)), "tcp no delay");

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  CHECK(bind(sock, (struct sockaddr*) &address, sizeof(address)), "bind failure");
  DEBUG_PRINT("socket bind complete\n");
  CHECK(listen(sock, QUEUE_LENGTH), "socket listen");
  DEBUG_PRINT("listening on %d\n", PORT);
  
  return sock;
}

/**
 * Opens up the waiting connection from the given socket.
 */
int process_accept(int sock_fd) {

  struct sockaddr_in in_addr;
  socklen_t size = sizeof(in_addr);
  int conn_sock = CHECK(accept(sock_fd, (struct sockaddr *) &in_addr, &size), "accept");
  fcntl(conn_sock, F_SETFL, O_NONBLOCK);
  DEBUG_PRINT("new request from %d:%d\n", in_addr.sin_addr.s_addr, in_addr.sin_port);
  return conn_sock;
}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void event_loop(int sock_fd) {
  int epoll_fd = CHECK(epoll_create(1), "epoll");

  struct epoll_event event;
  event.data.fd = sock_fd;
  event.events = EPOLLIN;

  CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event), "epoll control");
  
  struct epoll_event events[MAX_EVENTS];

  while (1) {
    int ready_amount = CHECK(epoll_wait(epoll_fd, events, MAX_EVENTS, -1), "epoll wait");

    for (int i = 0 ; i < ready_amount ; i++) {
      if (events[i].data.fd == sock_fd) {
	int conn_fd = process_accept(sock_fd);
	event.events = EPOLLIN;
	event.data.fd = conn_fd;
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event), "epoll control");
      } else {
	int fd = events[i].data.fd;
	char buffer[128];
	size_t count = read(fd, buffer, sizeof(buffer));
	DEBUG_PRINT("read %zu bytes\n", count);
	write(0, buffer, count);
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
      }
    }
  }  
}


int main(int argc, char* argv[]) {
  DEBUG_PRINT("starting up...\n");
  int sock_fd = create_socket();
  event_loop(sock_fd);
}
