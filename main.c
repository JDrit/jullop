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

/**
 * Opens up the waiting connection from the given socket.
 */
int process_accept(int sock_fd) {
  struct sockaddr_in in_addr;
  socklen_t size = sizeof(in_addr);
  int conn_sock = accept(sock_fd, (struct sockaddr *) &in_addr, &size);
  CHECK(conn_sock == -1, "failed to accept connection");
  fcntl(conn_sock, F_SETFL, O_NONBLOCK);

  char hbuf[NI_MAXHOST];
  char sbuf[NI_MAXSERV];

  int r = getnameinfo(&in_addr, size, hbuf, sizeof(hbuf), sbuf,
		      sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
  CHECK(r == -1, "Failed to get host name");
  
  LOG_DEBUG("new request from %s:%s", hbuf, sbuf);
  return conn_sock;
}

void close_request(struct epoll_event *event) {
  close(rc_get_fd((RequestContext*) event->data.ptr));
  destroy_request_context((RequestContext*) event->data.ptr);  
}

/**
 * Checks for any error condition after waiting on the epoll event.
 * Returns 0 for success, -1 in case of error.
 */
int handle_poll_errors(struct epoll_event *event) {
  if (event->events & EPOLLERR) {
    LOG_ERROR("EPOLLERR from event");
    close_request(event);
    return -1;
  } else if (event->events & EPOLLHUP) {
    LOG_ERROR("EPOLLHUP indicating peer closed the channel");
    close_request(event);
    return -1;
  } else {
    return 0;
  }
}

void read_socket(struct epoll_event *event) {
  int fd = rc_get_fd((RequestContext*) event->data.ptr);
  char buffer[256];
  int done = 0;
  
  while (1) {
    ssize_t count = read(fd, buffer, sizeof(buffer));
    LOG_ERROR("Read %zd from fd %d", count, fd);
    if (count == -1) {
      if (errno != EAGAIN) {
	done = 1;
      }
      break;
    } else if (count == 0) {
      done = 1;
      break;
    }
    
    write(1, buffer, count);
  }

  if (done) {
    LOG_INFO("Connection closed on fd %d", fd);
    close_request(event);
  }
}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void event_loop(int sock_fd) {
  int epoll_fd = epoll_create(1);
  CHECK(epoll_fd == -1, "failed to create epoll loop");

  struct epoll_event event;
  event.data.fd = sock_fd;
  event.events = EPOLLIN;

  CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event), "epoll control");
  
  struct epoll_event events[MAX_EVENTS];

  while (1) {
    int ready_amount = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "failed waiting on epoll");
    
    for (int i = 0 ; i < ready_amount ; i++) {
      if (handle_poll_errors(&events[i]) != 0) {
	continue;
      } else if (events[i].data.fd == sock_fd) {
	int conn_fd = process_accept(sock_fd);
	
	event.events = EPOLLIN;
	event.data.ptr = init_request_context(conn_fd);
	
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event) == -1,
	      "epoll control");

      } else if (events[i].events & EPOLLIN) {	
	read_socket(&events[i]);
      }
    }
  }  
}


int main(int argc, char* argv[]) {
  LOG_INFO("starting up...");
  int sock_fd = create_socket();
  event_loop(sock_fd);
}
