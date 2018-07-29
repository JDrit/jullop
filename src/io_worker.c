#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "epoll_info.h"
#include "input_buffer.h"
#include "io_worker.h"
#include "logging.h"
#include "message_passing.h"
#include "output_buffer.h"
#include "queue.h"
#include "request_context.h"
#include "time_stats.h"

#define MAX_EVENTS 4096

SocketContext *init_context(Server *server, EpollInfo *epoll_info) {
  SocketContext *socket_context = (SocketContext*) CHECK_MEM(malloc(sizeof(SocketContext)));
  socket_context->server = server;
  socket_context->epoll_info = epoll_info;
  return socket_context;
}

void handle_accept_read(SocketContext *context) {
  while (1) {
    struct sockaddr_in in_addr;
    socklen_t size = sizeof(in_addr);
    int conn_sock = accept(context->data.fd, (struct sockaddr *) &in_addr, &size);
    if (conn_sock == -1) {
      if (ERROR_BLOCK) {
	return;
      } else {
	FAIL("Failed to accept connection");
      }
    }

    int opt = 1;
    int r = setsockopt(conn_sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));
    CHECK(r == -1, "Failed to set options on TCP_NODELAY");

    r = setsockopt(conn_sock, SOL_TCP, TCP_QUICKACK, &opt, sizeof(opt));
    CHECK(r == -1, "Failed to set options on TCP_QUICKACK");
    
    fcntl(conn_sock, F_SETFL, O_NONBLOCK);
    
    char* hbuf = (char*) CHECK_MEM(calloc(NI_MAXHOST, sizeof(char)));
    char sbuf[NI_MAXSERV];
    
    r = getnameinfo((struct sockaddr*) &in_addr, size, hbuf,
		    NI_MAXHOST * sizeof(char), sbuf,
		    sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    CHECK(r == -1, "Failed to get host name");
    
    /* increments the counter of the total active requests. */
    stats_incr_active(context->server->stats);

    RequestContext *request_context = init_request_context(conn_sock, hbuf,
							   context->epoll_info);
    request_record_start(&request_context->time_stats, TOTAL_TIME);
    
    SocketContext *connection_context = init_context(context->server, context->epoll_info);
    connection_context->data.ptr = request_context;
    connection_context->input_handler = client_handle_read;
    connection_context->output_handler = NULL;
    connection_context->error_handler = client_handle_error;

    add_input_epoll_event(context->epoll_info, request_context->fd, connection_context);
  }
}

void handle_accept_error(SocketContext *context, uint32_t events) {
  LOG_ERROR("Accept socket ran into an error");
  FAIL("this should not happen");
}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void *io_event_loop(void *pthread_input) {
  IoWorkerArgs *args = (IoWorkerArgs*) pthread_input;
  int sock_fd = args->sock_fd;
  Server *server = args->server;

  /* make sure all application threads have started */
  pthread_barrier_wait(&server->startup);
  LOG_INFO("Starting IO thread");
    
  const char *name = "IO-Thread";
  EpollInfo *epoll_info = epoll_info_init(name, args->id);

  /* Adds the epoll event for listening for new connections */
  SocketContext *context = init_context(server, epoll_info);
  context->data.fd = sock_fd;
  context->input_handler = handle_accept_read;
  context->output_handler = NULL;
  context->error_handler = handle_accept_error;
  add_input_epoll_event(epoll_info, sock_fd, context);  

  struct epoll_event events[MAX_EVENTS];
  while (1) {
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);

    if (ready_amount == -1) {
      LOG_WARN("Failed to wait on epoll");
      continue;
    }
    
    for (int i = 0 ; i < ready_amount ; i++) {
      SocketContext *socket_context = (SocketContext*) events[i].data.ptr;

      if (events[i].events & EPOLLERR
	  || events[i].events & EPOLLRDHUP
	  || events[i].events & EPOLLHUP
	  || events[i].events & EPOLLPRI) {
	if (socket_context->error_handler != NULL) {
	  /* Multiple events can happen to a listener at the same time. We
           * should always check for error first and skip any other events
           * for the same listener. */
	  socket_context->error_handler(socket_context, events[i].events);
	} else {
	  LOG_WARN("epoll event did not have an error handler attached");
	}
	continue;
      }

      if (events[i].events & EPOLLIN) {
	if (socket_context->input_handler != NULL) {
	  socket_context->input_handler(socket_context);
	} else {
	  LOG_WARN("epoll event did not have an input handler attached");
	}
      }
      
      if (events[i].events & EPOLLOUT) {
	if (socket_context->output_handler != NULL) {
	  socket_context->output_handler(socket_context);
	} else {
	  LOG_WARN("epoll event did not have an output handler attached");
	}
      }
    }
  }
  return NULL;
}

