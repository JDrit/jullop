
#include <stdlib.h>

#include "epoll_info.h"
#include "logging.h"
#include "message_passing.h"
#include "request_context.h"
#include "server.h"
#include "time_stats.h"

#define MAX_EVENTS 10

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

typedef struct OutputContext {
  enum Type type;
  union Payload payload;
} OutputContext;

/**
 * Cleans up client requests.
 */
static void close_client_connection(EpollInfo *epoll_info,
				    OutputContext *output_context,
				    enum RequestResult result) {
  RequestContext *request_context = output_context->payload.request_context;
  
  epoll_info->stats.active_connections--;
  epoll_info->stats.bytes_written += context_bytes_written(request_context);
  delete_epoll_event(epoll_info, request_context->fd);

  // log the timestamp at which the request is done
  request_set_end(&request_context->time_stats);

  // finishes up the request
  request_finish_destroy(request_context, result);
  free(output_context);
}

static OutputContext *init_actor_output_context(int fd) {
  OutputContext *output_context = (OutputContext*) CHECK_MEM(calloc(1, sizeof(OutputContext)));
  ActorContext *actor_context = (ActorContext*) CHECK_MEM(calloc(1, sizeof(ActorContext)));
  actor_context->fd = fd;
  output_context->type = ACTOR;
  output_context->payload.actor_context = actor_context;
  return output_context;
}

static OutputContext *init_client_output_context(RequestContext *request_context) {
  OutputContext *output_context = (OutputContext*) CHECK_MEM(calloc(1, sizeof(OutputContext)));
  output_context->type = CLIENT;
  output_context->payload.request_context = request_context;
  return output_context;
}

/**
 * Takes the given request context and configures the event loop to start writing
 * out a response to the given client.
 */
static void register_client_output(EpollInfo *epoll_info,
				   RequestContext *request_context) {
  OutputContext *output_context = init_client_output_context(request_context);
  int fd = request_context->fd;
  add_output_epoll_event(epoll_info, fd, output_context);
}

static void read_actor_message(EpollInfo *epoll_info, OutputContext *output_context) {
  /* Since we are running the epoll event loop on edge-triggered, we need to read
   * all of the data off the socket after each wake up. This is the reason the 
   * following is in a loop. */
  while (1) {
    ActorContext *actor_context = output_context->payload.actor_context;
    enum ReadState state = message_read_async(actor_context->fd, &actor_context->message);
    
    switch (state) {
    case READ_BUSY:
      LOG_DEBUG("Not enough input from actor");
      return;
    case READ_ERROR:
      LOG_WARN("Error reading from actor");
      return;
    case CLIENT_DISCONNECT:
      LOG_WARN("Client disconnect from actor");
      return;
    case READ_FINISH: {
      /* Once the full message has been read from the application-level actor,
       * register the client's file descriptor to start writing the response out */
      RequestContext *request_context =
	(RequestContext*) message_get_payload(&actor_context->message);
      
      /* The request context was originally created by the input actor thread and
       * then modified by the application-level actor so we need to make sure that
       * the cache lines have been flused. */
      __sync_synchronize();
      register_client_output(epoll_info, request_context);
      message_reset(&actor_context->message);
      break;
    }
    }
  }
}

static void write_client_response(EpollInfo *epoll_info, OutputContext *output_context) {
  RequestContext *request_context = output_context->payload.request_context;
  
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
    close_client_connection(epoll_info, output_context, REQUEST_WRITE_ERROR);
    break;
  case WRITE_FINISH:
    request_context->state = FINISH;
    close_client_connection(epoll_info, output_context, REQUEST_SUCCESS);
    break;
  }
}

void *output_event_loop(void *pthread_input) {
  Server *server = (Server*) pthread_input;
  const char *name = "output actor";
  EpollInfo *epoll_info = init_epoll_info(name);

  /*
   * Registers each application-level actor pipeline so that the output actor
   * can start receiving responses to send back to the client.
   */
  for (int i = 0 ; i < server->actor_count ; i++) {
    int fd = server->app_actors[i].output_actor_fd;
    OutputContext *output_context = init_actor_output_context(fd);
    add_input_epoll_event(epoll_info, fd, output_context);
  }

  while (1) {
    struct epoll_event events[MAX_EVENTS];
    int timeout = 5 * 1000; // 10 seconds
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, timeout);
    CHECK(ready_amount == -1, "Error while waiting for epoll events");
    LOG_DEBUG("Output epoll events: %d", ready_amount);

    for (int i = 0 ; i < ready_amount ; i++) {
      if (check_epoll_errors(epoll_info, &events[i]) != NONE) {
	// handle client requests closes
	continue;
      }
      OutputContext *output_context = (OutputContext*) events[i].data.ptr;

      if (events[i].events & EPOLLIN && output_context->type == ACTOR) {
	read_actor_message(epoll_info, output_context);	
      }

      if (events[i].events & EPOLLOUT && output_context->type == CLIENT) {
	write_client_response(epoll_info, output_context);
      }
    }
  }

  return NULL;
}
