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

bool client_read_request(Server *server, EpollInfo *epoll_info, int id,
			 IoContext *io_context) {
  
  RequestContext *request_context = io_context->context.request_context;
  request_record_start(&request_context->time_stats, CLIENT_READ_TIME);
  enum ReadState state = try_parse_http_request(request_context);
  request_record_end(&request_context->time_stats, CLIENT_READ_TIME);

  switch (state) {
  case READ_FINISH: {
    /* We have to deregister the file descriptor from the event loop 
     * before we send it off or a race condition could cause it to be
     * closed before the input actor finishes deleting the event. */
    if (request_context->epoll_state != EPOLL_NONE) {
      request_context->epoll_state = EPOLL_NONE;
      delete_epoll_event(epoll_info, request_context->fd);
    }

    /* the next stage of the pipline is to forward the request to the
     * actor. */
    request_context->state = ACTOR_WRITE;

    size_t actor_id = (count++) % (size_t) server->actor_count;

    //todo fix this not to be blocking
    ActorInfo *actor_info = &server->app_actors[actor_id];
    Queue *input_queue = actor_info->input_queue[id];

    request_record_start(&request_context->time_stats, QUEUE_TIME);
    enum QueueResult queue_result = queue_push(input_queue, request_context);
    CHECK(queue_result != QUEUE_SUCCESS, "Failed to send message");
    
    free(io_context);
    return true;
  }
  case READ_ERROR:
    request_context->state = FINISH;
    client_close_connection(server, epoll_info, io_context, REQUEST_READ_ERROR);
    return true;
  case CLIENT_DISCONNECT:
    request_context->state = FINISH;
    client_close_connection(server, epoll_info, io_context, REQUEST_CLIENT_ERROR);
    return true;
  case READ_BUSY:
    // there is still more to read off of the client request, for now go
    // back onto the event loop
    return false;
  }
}

bool client_write_response(Server *server, EpollInfo *epoll_info,
			   IoContext *io_context) {
  RequestContext *request_context = io_context->context.request_context;
  
  request_record_start(&request_context->time_stats, CLIENT_WRITE_TIME);
  enum WriteState state = output_buffer_write_to(request_context->output_buffer,
						 request_context->fd);
  request_record_end(&request_context->time_stats, CLIENT_WRITE_TIME);
  
  switch (state) {
  case WRITE_BUSY:
    LOG_DEBUG("client write socket busy");
    return false;
  case WRITE_ERROR:
    LOG_WARN("Error while writing response to client");
    client_close_connection(server, epoll_info, io_context, REQUEST_WRITE_ERROR);
    return true;
  case WRITE_FINISH:
    request_context->state = FINISH;

    if (context_keep_alive(request_context) == 1) {
      client_reset_connection(server, epoll_info, io_context, REQUEST_SUCCESS);
    } else {
      client_close_connection(server, epoll_info, io_context, REQUEST_SUCCESS);
    }
    return true;
  }
}

void client_reset_connection(Server *server, EpollInfo *epoll_info,
			     IoContext *io_context, enum RequestResult result) {
  RequestContext *request_context = io_context->context.request_context;

  stats_incr_total(server->stats);
 
  // log the timestamp at which the request is done
  request_record_end(&request_context->time_stats, TOTAL_TIME);
  
  // finishes up the request
  context_finalize_reset(request_context, result);

  request_record_start(&request_context->time_stats, TOTAL_TIME);

  request_context->epoll_state = EPOLL_INPUT;
  switch (request_context->epoll_state) {
  case EPOLL_INPUT:
    break;
  case EPOLL_OUTPUT:
    mod_input_epoll_event(epoll_info, request_context->fd, io_context);
    break;
  case EPOLL_NONE:
    add_input_epoll_event(epoll_info, request_context->fd, io_context);
    break;
  }
}


void client_close_connection(Server *server, EpollInfo *epoll_info,
			     IoContext *io_context, enum RequestResult result) {
  
  RequestContext *request_context = io_context->context.request_context;

  stats_decr_active(server->stats);
  stats_incr_total(server->stats);
  
  // log the timestamp at which the request is done
  request_record_end(&request_context->time_stats, TOTAL_TIME);

  if (request_context->epoll_state != EPOLL_NONE) {
    request_context->epoll_state = EPOLL_NONE;
    delete_epoll_event(epoll_info, request_context->fd);
  }

  // finishes up the request
  context_finalize_destroy(request_context, result);  
  free(io_context);
}
