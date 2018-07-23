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

typedef struct ActorContext {
  /* the id of the actor. */
  int id;
  /* the event file descriptor used for messages from the actor. */
  int fd;
  /* the queue to receieve events back from the actor. */
  Queue *queue;
} ActorContext;

typedef struct AcceptContext {
  /* the file descriptor to listen for new connections on. */
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

static struct timespec last_print;
static uint64_t from_client = 0;
static uint64_t to_actor = 0;
static uint64_t from_actor = 0;
static uint64_t to_client = 0;

static void print_stats(Server *server, EpollInfo *epoll_info) {
  struct timespec cur_time;
  clock_gettime(CLOCK_MONOTONIC, &cur_time);

  if (cur_time.tv_sec - last_print.tv_sec > 5) {
    last_print = cur_time;

    size_t input_size = 0;
    size_t output_size = 0;
    
    for (int i = 0 ; i < server->actor_count ; i++) {
      input_size += queue_size(server->app_actors[i].input_queue);
      output_size += queue_size(server->app_actors[i].output_queue);
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
 * Cleans up client requests.
 */
static void close_client_connection(EpollInfo *epoll_info,
				    IoContext *io_context,
				    enum RequestResult result) {
  RequestContext *request_context = io_context->context.request_context;
  
  epoll_info->stats.active_connections--;
  epoll_info->stats.total_requests_processed++;
  epoll_info->stats.bytes_written += context_bytes_written(request_context);
  epoll_info->stats.bytes_read += context_bytes_read(request_context);

  // log the timestamp at which the request is done
  request_record_end(&request_context->time_stats, TOTAL_TIME);
    
  delete_epoll_event(epoll_info, request_context->fd);

  // finishes up the request
  context_finalize_destroy(request_context, result);  
  free(io_context);
}

/**
 * The client has asked for the socket to stay open for future requests. To
 * achieve this, the request context is reset and reused for future operations.
 */
static void reset_client_connection(EpollInfo *epoll_info,
				    IoContext *io_context,
				    enum RequestResult result) {
  RequestContext *request_context = io_context->context.request_context;

  epoll_info->stats.total_requests_processed++;
  epoll_info->stats.bytes_written += context_bytes_written(request_context);
  epoll_info->stats.bytes_read += context_bytes_read(request_context);

  // log the timestamp at which the request is done
  request_record_end(&request_context->time_stats, TOTAL_TIME);
  
  // finishes up the request
  context_finalize_reset(request_context, result);

  request_record_start(&request_context->time_stats, TOTAL_TIME);

  mod_input_epoll_event(epoll_info, request_context->fd, io_context);
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

    RequestContext *request_context = init_request_context(conn_sock, hbuf);
    request_record_start(&request_context->time_stats, TOTAL_TIME);
    
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
  size_t prev_len = request_context->input_buffer->offset;
  enum ReadState read_state = input_buffer_read_into(request_context->input_buffer,
						     request_context->fd);

  switch (read_state) {
  case READ_ERROR:
  case CLIENT_DISCONNECT:
    return read_state;
  case READ_FINISH:
  case READ_BUSY: {
    enum ParseState parse_state = http_request_parse(request_context->input_buffer,
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
  request_record_start(&request_context->time_stats, CLIENT_READ_TIME);
  enum ReadState state = try_parse_http_request(request_context);
  request_record_end(&request_context->time_stats, CLIENT_READ_TIME);

  switch (state) {
  case READ_FINISH: {
    from_client++;

    /* We have to deregister the file descriptor from the event loop 
     * before we send it off or a race condition could cause it to be
     * closed before the input actor finishes deleting the event. */
    delete_epoll_event(epoll_info, request_context->fd);

    /* the next stage of the pipline is to forward the request to the
     * actor. */
    request_context->state = ACTOR_WRITE;

    size_t actor_id = (count++) % (size_t) server->actor_count;

    //todo fix this not to be blocking
    ActorInfo *actor_info = &server->app_actors[actor_id];
    Queue *input_queue = actor_info->input_queue;

    request_record_start(&request_context->time_stats, QUEUE_TIME);
    enum QueueResult queue_result = queue_push(input_queue, request_context);
    CHECK(queue_result != QUEUE_SUCCESS, "Failed to send message");
    to_actor++;
    
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
  
  request_record_start(&request_context->time_stats, CLIENT_WRITE_TIME);
  enum WriteState state = output_buffer_write_to(request_context->output_buffer,
						 request_context->fd);
  request_record_end(&request_context->time_stats, CLIENT_WRITE_TIME);
  
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
    to_client++;

    if (context_keep_alive(request_context) == 1) {
      reset_client_connection(epoll_info, io_context, REQUEST_SUCCESS);
    } else {
      close_client_connection(epoll_info, io_context, REQUEST_SUCCESS);
    }
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

/**
 * Attempts to read from the actor's event file descriptor to see how many
 * request contexts are ready to be pulled off the queue. It then pulls that
 * many off and attaches them =to the epoll loop for client writes.
 *
 * Returns 0 on success and -1 on error.
 */
int read_queue_item(EpollInfo *epoll_info, Server *server, ActorContext *actor_context) {
  eventfd_t num_to_read;  
  int r = eventfd_read(actor_context->fd, &num_to_read);
  if (r == -1) {
    return -1;
  }

  for (eventfd_t i = 0 ; i < num_to_read ; i++) {
    RequestContext *request_context = (RequestContext*) queue_pop(actor_context->queue);
    if (request_context == NULL) {
      LOG_WARN("Failed to dequeue a request context");
      return -1;
    } else {
      request_record_end(&request_context->time_stats, QUEUE_TIME);
      from_actor++;
      
      /* All that is left is to send the response back to the client. */
      request_context->state = CLIENT_WRITE;
      
      /* Adds an epoll event responsible for writing the fully constructed HTTP 
       * response back to the client. */
      IoContext *client_output = init_client_io_context(request_context);
      add_output_epoll_event(epoll_info, request_context->fd, client_output);      
    }
  }
  return 0;
}

void handle_actor_op(EpollInfo *epoll_info, Server *server, IoContext *io_context) {
  ActorContext *actor_context = io_context->context.actor_context;

  while (1) {
    if (read_queue_item(epoll_info, server, actor_context) == -1) {
      return;
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

  // make sure all application threads have started
  pthread_barrier_wait(&server->startup);

  LOG_INFO("Starting IO thread");
    
  const char *name = "IO-Thread";
  EpollInfo *epoll_info = epoll_info_init(name);

  /* Adds the epoll event for listening for new connections */
  IoContext *accept_context = init_accept_io_context(sock_fd);  
  add_input_epoll_event(epoll_info, sock_fd, accept_context);

  /* Registers the output of the application-level actors so that responses
   * will be picked up and send back to the clients. */
  for (int i = 0 ; i < server->actor_count ; i++) {
    Queue *output_queue = server->app_actors[i].output_queue;
    int fd = queue_add_event_fd(output_queue);
    
    IoContext *io_context = init_actor_io_context(i, fd, output_queue);
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

