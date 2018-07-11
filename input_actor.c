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
  Message message;
} ActorContext;

enum Type {
  CLIENT,
  ACTOR,
};

union Payload {
  RequestContext *request_context;
  ActorContext *actor_context;
};

typedef struct InputContext {
  enum Type type;
  union Payload payload;
} InputContext;

/**
 * Cleans up client requests.
 */
static void close_client_connection(EpollInfo *epoll_info,
				    InputContext *input_context,
				    enum RequestResult result) {
  
  RequestContext *request_context = input_context->payload.request_context;
  
  epoll_info->stats.active_connections--;
  epoll_info->stats.bytes_read += context_bytes_read(request_context);
  delete_epoll_event(epoll_info, request_context->fd);

  // log the timestamp at which the request is done
  request_set_end(&request_context->time_stats);

  // finishes up the request
  request_finish_destroy(request_context, result);
  free(input_context);
}

static InputContext *init_actor_input_context(int fd) {
  InputContext *input_context = (InputContext*) CHECK_MEM(calloc(1, sizeof(InputContext)));
  ActorContext *actor_context = (ActorContext*) CHECK_MEM(calloc(1, sizeof(ActorContext)));
  actor_context->fd = fd;
  input_context->type = ACTOR;
  input_context->payload.actor_context = actor_context;
  return input_context;
}

static InputContext *init_client_input_context(RequestContext *request_context) {
  InputContext *input_context = (InputContext*) CHECK_MEM(calloc(1, sizeof(InputContext)));
  input_context->type = CLIENT;
  input_context->payload.request_context = request_context;
  return input_context;
}

/**
 * Accepts as many as possible waiting connections and registers them all in the 
 * input event loop. This can register multiple connections in one go.
 */
void process_accepts(EpollInfo *epoll_info, int accept_fd) {

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

    int fd = request_context->fd;
    InputContext *input_context = init_client_input_context(request_context);
    add_input_epoll_event(epoll_info, fd, input_context);    
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
    }
  }
  }
}

static size_t count = 0;

/**
 * Handles when the client socket is ready to be read from. Reads as much as
 * possible until a full HTTP request has been parsed.
 */
static void read_client_request(EpollInfo *epoll_info,
				Server *server,
				InputContext *input_context) {
  RequestContext *request_context = input_context->payload.request_context;

  request_set_client_read_start(&request_context->time_stats);
  enum ReadState state = try_parse_http_request(request_context);  
  request_set_client_read_end(&request_context->time_stats);

  switch (state) {
  case READ_FINISH: {

    /* We have to deregister the file descriptor from the event loop 
     * before we send it off or a race condition could cause it to be
     * closed before the input actor finishes deleting the event. */
    delete_epoll_event(epoll_info, request_context->fd);

    epoll_info->stats.active_connections--;

    size_t actor_id = (count++) % (size_t) server->actor_count;

    //todo fix this not to be blocking
    ActorInfo *actor_info = &server->app_actors[actor_id];
    int fd = actor_info->input_actor_fd;
    
    Message message;
    message_init(&message, request_context);

    enum WriteState write_state = message_write_async(fd, &message);
    CHECK(write_state != WRITE_FINISH, "Failed to write message");

    // clean up the context used for reading client input.
    free(input_context);
    
    return;
  }
  case READ_ERROR:
    request_context->state = FINISH;
    close_client_connection(epoll_info, input_context, REQUEST_READ_ERROR);
    return;
  case CLIENT_DISCONNECT:
    request_context->state = FINISH;
    close_client_connection(epoll_info, input_context, REQUEST_CLIENT_ERROR);
    return;
  case READ_BUSY:
    // there is still more to read off of the client request, for now go
    // back onto the event loop
    return;
  }
}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void *input_event_loop(void *pthread_input) {
  Server *server = (Server*) pthread_input;
  int sock_fd = server->sock_fd;
  const char *name = "input actor";
  EpollInfo *epoll_info = init_epoll_info(name);
  set_accept_epoll_event(epoll_info, sock_fd);

  /*
   * Registers each application-level actor input socket so that the input
   * actor can start to send requests to it.
   */
  /*
  for (int i = 0 ; i < server->actor_count ; i++) {
    int fd = server->app_actors[i].input_actor_fd;
    InputContext *input_context = init_actor_input_context(fd);
    add_output_epoll_event(epoll_info, fd, input_context);
  }
  */

  size_t max_active = 0;
  
  while (1) {
    struct epoll_event events[MAX_EVENTS];
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "Failed waiting on epoll");

    LOG_DEBUG("Input actor received %d events", ready_amount);

    if (epoll_info->stats.active_connections > max_active) {
      max_active = epoll_info->stats.active_connections;
    }
    
    for (int i = 0 ; i < ready_amount ; i++) {
      
      if (events[i].data.ptr == epoll_info) {
	if (check_epoll_errors(epoll_info, &events[i]) == NONE) {
	  process_accepts(epoll_info, sock_fd);
	} else {
	  // error on accepting new connections
	  FAIL("Epoll error while accepting new connections");
	}
      } else {
	InputContext *input_context = (InputContext*) events[i].data.ptr;
	if (input_context->type == CLIENT) {
	  if (check_epoll_errors(epoll_info, &events[i]) == NONE) {
	    read_client_request(epoll_info, server, input_context);
	  } else {
	    /* The epoll event controlling the client connection ran 
             * into an issue. Disconnect and end the connection. */
	    int fd = input_context->payload.request_context->fd;
	    int error = 0;
	    socklen_t errlen = sizeof(error);
	    getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);
	    LOG_WARN("Closing connection early due to %s", strerror(error));	    
	    close_client_connection(epoll_info, input_context, REQUEST_EPOLL_ERROR);
	  }
	} else if (input_context->type == ACTOR) {
	  // todo: handle actor writes
	} else {
	  FAIL("Unknown input context type");
	}
      }
    }
  }
  return NULL;
}

