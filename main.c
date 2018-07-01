#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include "logging.h"
#include "request_context.h"
#include "event_loop.h"

#define PORT 8080
#define QUEUE_LENGTH 10
#define MAX_EVENTS 10

int create_socket() {
  int opt = 1;
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  CHECK(sock == -1, "failed to get socket");

  // allows reusing of the same address and port
  CHECK(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &opt, sizeof(opt)) == -1,
	"Failed to set options on SO_REUSEPORT | SO_REUSEADDR");
  // all writes should send packets right away
  CHECK(setsockopt(sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1,
	"Failed to set options on TCP_NODELAY");

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  CHECK(bind(sock, (struct sockaddr*) &address, sizeof(address)) == -1,
	"bind failure to %d", PORT);
  LOG_DEBUG("socket bind complete");
  CHECK(listen(sock, QUEUE_LENGTH), "socket listen");
  LOG_DEBUG("listening on %d", PORT);
  
  return sock;
}

int main(int argc, char* argv[]) {
  LOG_INFO("starting up...");
  int sock_fd = create_socket();
  EventLoop* eventLoop = init_event_loop(sock_fd);
  event_loop(eventLoop);
}
