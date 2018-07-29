#define _GNU_SOURCE

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "client.h"
#include "epoll_info.h"
#include "input_buffer.h"
#include "io_worker.h"
#include "logging.h"
#include "output_buffer.h"
#include "queue.h"
#include "request_context.h"
#include "server.h"
#include "time_stats.h"

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

void client_handle_read(SocketContext *context) {
  Server *server = context->server;
  EpollInfo *epoll_info = context->epoll_info;
  RequestContext *request_context = (RequestContext*) context->data.ptr;
  
  request_record_start(&request_context->time_stats, CLIENT_READ_TIME);
  enum ReadState state = try_parse_http_request(request_context);
  request_record_end(&request_context->time_stats, CLIENT_READ_TIME);

  switch (state) {
  case READ_FINISH: {
    /* We have to deregister the file descriptor from the event loop 
     * before we send it off or a race condition could cause it to be
     * closed before the input actor finishes deleting the event. */
    delete_epoll_event(epoll_info, request_context->fd);

    size_t actor_id = (count++) % (size_t) server->actor_count;

    //todo fix this not to be blocking
    ActorInfo *actor_info = &server->app_actors[actor_id];
    Queue *input_queue = actor_info->input_queue[epoll_info->id];

    request_record_start(&request_context->time_stats, QUEUE_TIME);
    enum QueueResult queue_result = queue_push(input_queue, request_context);
    CHECK(queue_result != QUEUE_SUCCESS, "Failed to send message");

    /* cleans up the context that was used for reading data in. */
    free(context);

    return;
  }
  case READ_ERROR:
    client_close_connection(context, REQUEST_READ_ERROR);    
    return;
  case CLIENT_DISCONNECT:
    client_close_connection(context, REQUEST_CLIENT_ERROR);
    return;
  case READ_BUSY:
    // there is still more to read off of the client request, for now go
    // back onto the event loop
    return;
  }
}

void client_handle_write(SocketContext *context) {
  RequestContext *request_context = (RequestContext*) context->data.ptr;
    
  request_record_start(&request_context->time_stats, CLIENT_WRITE_TIME);
  enum WriteState state = output_buffer_write_to(request_context->output_buffer,
						 request_context->fd);
  request_record_end(&request_context->time_stats, CLIENT_WRITE_TIME);
  
  switch (state) {
  case WRITE_BUSY:
    LOG_DEBUG("client write socket busy");
    return;
  case WRITE_ERROR:
    LOG_WARN("Error while writing response to client");
    client_close_connection(context, REQUEST_WRITE_ERROR);
    return;
  case WRITE_FINISH:
    if (context_keep_alive(request_context) == 1) {
      client_reset_connection(context, REQUEST_SUCCESS);
    } else {
      client_close_connection(context, REQUEST_SUCCESS);
    }
    return;
  }
}

void client_handle_error(SocketContext *context, uint32_t events) {

  if (events & EPOLLERR) {
    LOG_WARN("Error due to read size of socket closing");
  } else if (events & EPOLLHUP) {
    LOG_WARN("Error due to hang up on file descriptor");    
  } else if (events & EPOLLRDHUP) {
    LOG_WARN("Error due to peer closed connection");
  } else if (events & EPOLLPRI) {
    LOG_WARN("Error due to uknown issue");
  }

  context_print_finish((RequestContext*) context->data.ptr, REQUEST_EPOLL_ERROR);
  
  client_close_connection(context, REQUEST_EPOLL_ERROR);
}

void client_reset_connection(SocketContext *context, enum RequestResult result) {
  Server *server = context->server;
  EpollInfo *epoll_info = context->epoll_info;
  RequestContext *request_context = context->data.ptr;

  stats_incr_total(server->stats);
 
  // log the timestamp at which the request is done
  request_record_end(&request_context->time_stats, TOTAL_TIME);
  
  // finishes up the request
  context_finalize_reset(request_context, result);

  request_record_start(&request_context->time_stats, TOTAL_TIME);

  /* resets the context to start accept input from client again. */
  context->input_handler = client_handle_read;
  context->output_handler = NULL;
  context->error_handler = client_handle_error;

  mod_input_epoll_event(epoll_info, request_context->fd, context);
}

void client_close_connection(SocketContext *context, enum RequestResult result) {
  Server *server = context->server;
  EpollInfo *epoll_info = context->epoll_info;
  RequestContext *request_context = (RequestContext*) context->data.ptr;

  stats_decr_active(server->stats);
  stats_incr_total(server->stats);
  
  // log the timestamp at which the request is done
  request_record_end(&request_context->time_stats, TOTAL_TIME);

  delete_epoll_event(epoll_info, request_context->fd);

  // finishes up the request
  context_finalize_destroy(request_context, result);
  free(context);
}
