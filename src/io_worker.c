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

#define MAX_EVENTS 750

static struct timespec last_print;

static void print_stats(Server *server, EpollInfo *epoll_info) {
  struct timespec cur_time;
  clock_gettime(CLOCK_MONOTONIC, &cur_time);

  if (cur_time.tv_sec - last_print.tv_sec > 5) {
    last_print = cur_time;

    size_t input_size = 0;
    size_t output_size = 0;
    
    for (int actor_id = 0 ; actor_id < server->actor_count ; actor_id++) {
      for (int queue_id = 0 ; queue_id < server->io_worker_count ; queue_id++) {
	input_size += queue_size(server->app_actors[actor_id].input_queue[queue_id]);
	output_size += queue_size(server->app_actors[actor_id].output_queue[queue_id]);
      }
    }
    LOG_INFO("-------------------------------------------------------");
    LOG_INFO("Stats: total: %lu active: %lu",
	     epoll_info->stats.total_requests_processed,
	     epoll_info->stats.active_connections);    
    LOG_INFO("Actor: input: %lu output: %lu", input_size, output_size);
  }
  
}

static inline const char* context_type_name(IoContext *io_context) {
  switch (io_context->type) {
  case CLIENT: return "CLIENT";
  case ACTOR: return "ACTOR";
  case ACCEPT: return "ACCEPT";
  default: return "UNKNOWN";
  }
}

/**
 * The epoll event controlling the client connection can into an issue. 
 * Disconnect and end the connection. 
 */
static void handle_epoll_client_error(EpollInfo *epoll_info,
				      IoContext *io_context) {
  int fd = io_context->context.request_context->fd;
  int error = 0;
  socklen_t errlen = sizeof(error);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);
  LOG_WARN("Closing connection early due to %s", strerror(error));	    
  client_close_connection(epoll_info, io_context, REQUEST_EPOLL_ERROR);
}

static IoContext *init_actor_io_context(int id, int fd, Queue *queue) {
  IoContext *io_context = (IoContext*) CHECK_MEM(calloc(1, sizeof(IoContext)));
  ActorContext *actor_context = (ActorContext*) CHECK_MEM(calloc(1, sizeof(ActorContext)));
  actor_context->id = id;
  actor_context->fd = fd;
  actor_context->queue = queue;
  io_context->type = ACTOR;
  io_context->context.actor_context = actor_context;
  return io_context;
}

static IoContext *init_client_io_context(RequestContext *request_context) {
  IoContext *io_context = (IoContext*) CHECK_MEM(calloc(1, sizeof(IoContext)));
  io_context->type = CLIENT;
  io_context->context.request_context = request_context;
  return io_context;
}

static IoContext *init_accept_io_context(int fd) {
  IoContext *io_context = (IoContext*) CHECK_MEM(calloc(1, sizeof(IoContext)));
  AcceptContext *accept_context = (AcceptContext*) CHECK_MEM(calloc(1, sizeof(AcceptContext)));
  accept_context->fd = fd;
  io_context->type = ACCEPT;
  io_context->context.accept_context = accept_context;
  return io_context;
}

/**
 * Accepts as many as possible waiting connections and registers them all in the 
 * input event loop. This can register multiple connections in one go.
 */
void handle_accept_op(EpollInfo *epoll_info, Server *server, int accept_fd) {

  while (1) {
    struct sockaddr_in in_addr;
    socklen_t size = sizeof(in_addr);
    int conn_sock = accept(accept_fd, (struct sockaddr *) &in_addr, &size);
    if (conn_sock == -1) {
      if (ERROR_BLOCK) {
	return;
      } else {
	FAIL("Failed to accept connection");
      }
    }

    int opt = 1;
    CHECK(setsockopt(conn_sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1,
	  "Failed to set options on TCP_NODELAY");

    CHECK(setsockopt(conn_sock, SOL_TCP, TCP_QUICKACK, &opt, sizeof(opt)) == -1,
	  "Failed to set options on TCP_QUICKACK");
    
    fcntl(conn_sock, F_SETFL, O_NONBLOCK);
    
    char* hbuf = (char*) CHECK_MEM(calloc(NI_MAXHOST, sizeof(char)));
    char sbuf[NI_MAXSERV];
    
    int r = getnameinfo((struct sockaddr*) &in_addr, size, hbuf,
			NI_MAXHOST * sizeof(char), sbuf,
			sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    CHECK(r == -1, "Failed to get host name");
    
    LOG_DEBUG("New request from %s:%s", hbuf, sbuf);
    epoll_info->stats.active_connections++;

    RequestContext *request_context = init_request_context(conn_sock, hbuf);
    request_record_start(&request_context->time_stats, TOTAL_TIME);
    
    IoContext *io_context = init_client_io_context(request_context);

    /* does not wait for the epoll event to trigger before starting to read
     * from the request from the socket. There might already be data waiting
     * that the client has sent. */

    request_context->epoll_state = EPOLL_INPUT;
    add_input_epoll_event(epoll_info, request_context->fd, io_context);

  }
}

void handle_client_op(EpollInfo *epoll_info, Server *server, int id, IoContext *io_context) {
  RequestContext *request_context = io_context->context.request_context;
  switch (request_context->state) {
  case CLIENT_READ:
    client_read_request(epoll_info, server, id, io_context);
    break;
  case CLIENT_WRITE:
    client_write_response(epoll_info, io_context);
    break;
  default:
    LOG_WARN("Unknown request state: %d", request_context->state);
    break;
  }
}


void handle_actor_op(EpollInfo *epoll_info, Server *server, IoContext *io_context) {
  ActorContext *actor_context = io_context->context.actor_context;

  while (1) {
    eventfd_t num_to_read;  
    int r = eventfd_read(actor_context->fd, &num_to_read);
    if (r == -1) {
      return;
    }

    for (eventfd_t i = 0 ; i < num_to_read ; i++) {
      RequestContext *request_context = (RequestContext*) queue_pop(actor_context->queue);
      if (request_context == NULL) {
	LOG_WARN("Failed to dequeue a request context");
	return;
      } else {
	request_record_end(&request_context->time_stats, QUEUE_TIME);
	
	/* All that is left is to send the response back to the client. */
	request_context->state = CLIENT_WRITE;
	
	/* Adds an epoll event responsible for writing the fully constructed HTTP 
	 * response back to the client. */
	IoContext *client_output = init_client_io_context(request_context);
	request_context->epoll_state = EPOLL_OUTPUT;
	add_output_epoll_event(epoll_info, request_context->fd, client_output);
      }
    }
  }
}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void *io_event_loop(void *pthread_input) {
  clock_gettime(CLOCK_MONOTONIC, &last_print);
  IoWorkerArgs *args = (IoWorkerArgs*) pthread_input;
  int sock_fd = args->sock_fd;
  Server *server = args->server;

  /* make sure all application threads have started */
  pthread_barrier_wait(&server->startup);
  LOG_INFO("Starting IO thread");
    
  const char *name = "IO-Thread";
  EpollInfo *epoll_info = epoll_info_init(name);

  /* Adds the epoll event for listening for new connections */
  IoContext *accept_context = init_accept_io_context(sock_fd);  
  add_input_epoll_event(epoll_info, sock_fd, accept_context);

  /* Registers the output of the application-level actors so that responses
   * will be picked up and send back to the clients. */
  for (int actor_id = 0 ; actor_id < server->actor_count ; actor_id++) {
    Queue *output_queue = server->app_actors[actor_id].output_queue[args->id];
    int fd = queue_add_event_fd(output_queue);
    
    IoContext *io_context = init_actor_io_context(actor_id, fd, output_queue);
    add_input_epoll_event(epoll_info, fd, io_context);
  }
  
  struct epoll_event events[MAX_EVENTS];
  while (1) {
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, 5000);
    print_stats(server, epoll_info);

    if (ready_amount == -1) {
      LOG_WARN("Failed to wait on epoll");
      continue;
    }
    
    for (int i = 0 ; i < ready_amount ; i++) {
      IoContext *io_context = (IoContext*) events[i].data.ptr;
      LOG_DEBUG("Epoll event type: %s", context_type_name(io_context));
      
      //
      // handle epoll errors. 
      //
      if (check_epoll_errors(epoll_info, &events[i]) != NONE) {
	switch (io_context->type) {
	case ACTOR:
	  LOG_WARN("Epoll error for actor socket");
	  break;
	case ACCEPT:
	  LOG_WARN("Epoll error for accept socket");
	  break;
	case CLIENT:
	  handle_epoll_client_error(epoll_info, io_context);
	  break;
	}
	continue;
      }

      // -----------------------------------------------------------------------

      switch (io_context->type) {
      case ACTOR:
	handle_actor_op(epoll_info, server, io_context);
	break;
      case ACCEPT:
	handle_accept_op(epoll_info, server, io_context->context.accept_context->fd);
	break;
      case CLIENT:
	handle_client_op(epoll_info, server, args->id, io_context);
	break;
      }
    }
  }
  return NULL;
}

