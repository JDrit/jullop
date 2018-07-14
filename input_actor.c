#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "epoll_info.h"
#include "input_actor.h"
#include "logging.h"
#include "message_passing.h"
#include "request_context.h"
#include "time_stats.h"

#define MAX_EVENTS 250

typedef struct ActorContext {
  int fd;
} ActorContext;

typedef struct AcceptContext {
  int fd;
} AcceptContext;

typedef struct IoContext {

  enum type {
    CLIENT,
    ACTOR,
    ACCEPT,
  } type;

  union context {
    RequestContext *request_context;
    ActorContext *actor_context;
    AcceptContext *accept_context;
  } context;
  
} IoContext;

static inline const char* context_type_name(IoContext *io_context) {
  switch (io_context->type) {
  case CLIENT: return "CLIENT";
  case ACTOR: return "ACTOR";
  case ACCEPT: return "ACCEPT";
  default: return "UNKNOWN";
  }
}

/**
 * Cleans up client requests.
 */
static void close_client_connection(EpollInfo *epoll_info,
				    IoContext *io_context,
				    enum RequestResult result) {
  RequestContext *request_context = io_context->context.request_context;
  
  epoll_info->stats.active_connections--;
  epoll_info->stats.bytes_read += context_bytes_read(request_context);
  delete_epoll_event(epoll_info, request_context->fd);

  // log the timestamp at which the request is done
  request_set_end(&request_context->time_stats);

  // finishes up the request
  request_finish_destroy(request_context, result);
  free(io_context);
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
  close_client_connection(epoll_info, io_context, REQUEST_EPOLL_ERROR);
}

static IoContext *init_actor_io_context(int fd) {
  IoContext *io_context = (IoContext*) CHECK_MEM(calloc(1, sizeof(IoContext)));
  ActorContext *actor_context = (ActorContext*) CHECK_MEM(calloc(1, sizeof(ActorContext)));
  actor_context->fd = fd;
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
void handle_accept_op(EpollInfo *epoll_info, int accept_fd) {

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
    
    fcntl(conn_sock, F_SETFL, O_NONBLOCK);
    
    char* hbuf = (char*) CHECK_MEM(calloc(NI_MAXHOST, sizeof(char)));
    char sbuf[NI_MAXSERV];
    
    int r = getnameinfo((struct sockaddr*) &in_addr, size, hbuf,
			NI_MAXHOST * sizeof(char), sbuf,
			sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    CHECK(r == -1, "Failed to get host name");
    
    LOG_DEBUG("New request from %s:%s", hbuf, sbuf);
    epoll_info->stats.active_connections++;
    epoll_info->stats.total_requests_processed++;

    RequestContext *request_context = init_request_context(conn_sock, hbuf);
    request_set_start(&request_context->time_stats);
    
    IoContext *io_context = init_client_io_context(request_context);
    add_input_epoll_event(epoll_info, request_context->fd, io_context);
  }
}

/**
 * Reads data off of the client's file descriptor and attempts to parse a HTTP
 * request off of it. A READ_FINISH indicates that a full http request has
 * been successfully parsed.
 */
static enum ReadState try_parse_http_request(RequestContext *request_context) {
  size_t prev_len = request_context->input_offset;
  enum ReadState read_state = read_async(request_context->fd,
					 request_context->input_buffer,
					 request_context->input_len,
					 &request_context->input_offset);

  switch (read_state) {
  case READ_ERROR:
  case CLIENT_DISCONNECT:
    return read_state;
  case READ_FINISH:
  case READ_BUSY: {
    enum ParseState parse_state = http_request_parse(request_context->input_buffer,
						     request_context->input_offset,
						     prev_len,
						     &request_context->http_request);
    switch (parse_state) {
    case PARSE_FINISH:
      return READ_FINISH;
    case PARSE_ERROR:
      return READ_ERROR;
    case PARSE_INCOMPLETE:
      return READ_BUSY;
    default:
      return READ_ERROR;
    }
  }
  default:
    return READ_ERROR;
  }
}

static size_t count = 0;

/**
 * Handles when the client socket is ready to be read from. Reads as much as
 * possible until a full HTTP request has been parsed.
 */
static void read_client_request(EpollInfo *epoll_info,
				Server *server,
				IoContext *io_context) {
  RequestContext *request_context = io_context->context.request_context;

  request_set_client_read_start(&request_context->time_stats);
  enum ReadState state = try_parse_http_request(request_context);  
  request_set_client_read_end(&request_context->time_stats);

  switch (state) {
  case READ_FINISH: {

    /* We have to deregister the file descriptor from the event loop 
     * before we send it off or a race condition could cause it to be
     * closed before the input actor finishes deleting the event. */
    delete_epoll_event(epoll_info, request_context->fd);

    /* the next stage of the pipline is to forward the request to the
     * actor. */
    request_context->state = ACTOR_WRITE;

    epoll_info->stats.active_connections--;

    size_t actor_id = (count++) % (size_t) server->actor_count;

    //todo fix this not to be blocking
    ActorInfo *actor_info = &server->app_actors[actor_id];
    int fd = actor_info->input_actor_fd;
    
    Message message;
    message_init(&message, request_context);

    enum WriteState write_state = message_write_async(fd, &message);
    CHECK(write_state != WRITE_FINISH, "Failed to write message");

    free(io_context);

    return;
  }
  case READ_ERROR:
    request_context->state = FINISH;
    close_client_connection(epoll_info, io_context, REQUEST_READ_ERROR);
    return;
  case CLIENT_DISCONNECT:
    request_context->state = FINISH;
    close_client_connection(epoll_info, io_context, REQUEST_CLIENT_ERROR);
    return;
  case READ_BUSY:
    // there is still more to read off of the client request, for now go
    // back onto the event loop
    return;
  }
}

static void write_client_response(EpollInfo *epoll_info, IoContext *io_context) {
  RequestContext *request_context = io_context->context.request_context;
  
  request_set_client_write_start(&request_context->time_stats);
  enum WriteState state = write_async(request_context->fd,
				      request_context->output_buffer,
				      request_context->output_len,
				      &request_context->output_offset);
  request_set_client_write_end(&request_context->time_stats);
  
  switch (state) {
  case WRITE_BUSY:
    LOG_DEBUG("client write socket busy");
    break;
  case WRITE_ERROR:
    LOG_WARN("Error while writing response to client");
    close_client_connection(epoll_info, io_context, REQUEST_WRITE_ERROR);
    break;
  case WRITE_FINISH:
    request_context->state = FINISH;
    close_client_connection(epoll_info, io_context, REQUEST_SUCCESS);
    break;
  }
}

void handle_client_op(EpollInfo *epoll_info, Server *server, IoContext *io_context) {
  RequestContext *request_context = io_context->context.request_context;
  
  switch (request_context->state) {
  case CLIENT_READ:
    read_client_request(epoll_info, server, io_context);
    break;
  case CLIENT_WRITE:
    write_client_response(epoll_info, io_context);
    break;
  default:
    LOG_WARN("Unknown request state: %d", request_context->state);
    break;
  }
}

void handle_actor_op(EpollInfo *epoll_info, Server *server, IoContext *io_context) {
  ActorContext *actor_context = io_context->context.actor_context;

  Message message;
  message_reset(&message);
  enum ReadState state = message_read_async(actor_context->fd, &message);

  switch (state) {
  case READ_BUSY:
    LOG_DEBUG("Actor socket busy");
    break;
  case CLIENT_DISCONNECT:
  case READ_ERROR:
    LOG_WARN("Error while reading response from actor");
    break;
  case READ_FINISH: {
    
    /* Once the full message has been read from the application-level actor,
     * register the client's file descriptor to start writing the response out */    
    RequestContext *request_context = (RequestContext*) message_get_payload(&message);

    /* The request context was originally created by the input actor thread and
     * then modified by the application-level actor so we need to make sure that
     * the cache lines have been flused. */
    __sync_synchronize();

    /* All that is left is to send the response back to the client. */
    request_context->state = CLIENT_WRITE;
    
    /* Adds an epoll event responsible for writing the fully constructed HTTP 
     * response back to the client. */
    IoContext *client_output = init_client_io_context(request_context);
    add_output_epoll_event(epoll_info, request_context->fd, client_output);
  }
  }

}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void *input_event_loop(void *pthread_input) {
  Server *server = (Server*) pthread_input;

  // make sure all application threads have started
  pthread_barrier_wait(&server->startup);

  LOG_INFO("Starting IO thread");
  
  
  const char *name = "IO-Thread";
  EpollInfo *epoll_info = init_epoll_info(name);

  /* Adds the epoll event for listening for new connections */
  IoContext *accept_context = init_accept_io_context(server->sock_fd);  
  add_input_epoll_event(epoll_info, server->sock_fd, accept_context);

  /* Registers the output of the application-level actors so that responses
   * will be picked up and send back to the clients. */
  for (int i = 0 ; i < server->actor_count ; i++) {
    int fd = server->app_actors[i].output_actor_fd;
    IoContext *io_context = init_actor_io_context(fd);
    add_input_epoll_event(epoll_info, fd, io_context);
  }

  while (1) {
    struct epoll_event events[MAX_EVENTS];
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);
    LOG_DEBUG("Received %d epoll events", ready_amount);
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
	handle_accept_op(epoll_info, io_context->context.accept_context->fd);
	break;
      case CLIENT:
	handle_client_op(epoll_info, server, io_context);
	break;
      }
    }
  }
  return NULL;
}

