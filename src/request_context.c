#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "http_request.h"
#include "input_buffer.h"
#include "logging.h"
#include "output_buffer.h"
#include "request_context.h"
#include "request_stats.h"
#include "server.h"

#define BUF_SIZE 1024

inline static char* request_result_name(enum RequestResult result) {
  switch (result) {
  case REQUEST_SUCCESS:
    return "SUCCESS";
  case REQUEST_EPOLL_ERROR:
    return "EPOLL_ERROR";
  case REQUEST_READ_ERROR:
    return "READ_ERROR";
  case REQUEST_CLIENT_ERROR:
    return "CLIENT_ERROR";
  case REQUEST_WRITE_ERROR:
    return "WRITE_ERROR";
  default:
    return "UNKNOWN";
  }
}

RequestContext *init_request_context(int fd, char* host_name, EpollInfo* epoll_info) {
  RequestContext *context =
    (RequestContext*) CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->remote_host = host_name;
  context->fd = fd;
  context->actor_id = -1;

  context->input_buffer = input_buffer_init(BUF_SIZE);
  context->output_buffer = output_buffer_init(BUF_SIZE);

  context->http_request.num_headers = NUM_HEADERS;
  per_request_clear_time(&context->time_stats);

  context->epoll_info = epoll_info;
  
  return context;
}

size_t context_bytes_read(RequestContext *context) {
  return context->input_buffer->offset;
}

size_t context_bytes_written(RequestContext *context) {
  return context->output_buffer->write_from_offset;
}

int context_keep_alive(RequestContext *context) {
  HttpRequest http_request = context->http_request;

  for (size_t i = 0 ; i < http_request.num_headers ; i++) {
    struct phr_header header = http_request.headers[i];

    // "Connection" is 10 characters long
    if (header.name_len != 10) {
      continue;
    }
    
    if (strncmp(header.name, "Connection", 10) != 0) {
      continue;
    }

    // "Keep-Alive" is 10 characters long
    if (header.value_len != 10) {
      continue;
    }

    if (strncmp(header.value, "Keep-Alive", 10) != 0) {
      continue;
    }

    return 1;
  }
  return 0;
}

void context_print_finish(RequestContext *context, enum RequestResult result) {  
  LOG_DEBUG("\n"
	   "Request Stats : result=%s fd=%d remote_host=%s actor=%d "
	   "bytes_read=%zu bytes_written=%zu\n"
	   "Time Stats    : total_micros=%ld client_read_micros=%ld "
	   "actor_micros=%ld client_write_micros=%ld queue_micros=%ld",
	   request_result_name(result),
	   context->fd,
	   context->remote_host,
	   context->actor_id,
	   context->input_buffer->offset,
	   context->output_buffer->write_into_offset,
	   per_request_get_time(&context->time_stats, TOTAL_TIME),
	   per_request_get_time(&context->time_stats, CLIENT_READ_TIME),
	   per_request_get_time(&context->time_stats, ACTOR_TIME),
	   per_request_get_time(&context->time_stats, CLIENT_WRITE_TIME),
	   per_request_get_time(&context->time_stats, QUEUE_TIME));
}

void context_finalize_reset(RequestContext *context, enum RequestResult result) {
  context_print_finish(context, result);

  context->actor_id = -1;
  
  // reset the input buffer
  input_buffer_reset(context->input_buffer);
  output_buffer_reset(context->output_buffer);
  
  per_request_clear_time(&context->time_stats);
}

void context_finalize_destroy(RequestContext *context, enum RequestResult result) {
  context_print_finish(context, result);

  // free allocated memory for the request
  input_buffer_destroy(context->input_buffer);
  output_buffer_destroy(context->output_buffer);

  // free the buffer for the address of the remote hosr
  free(context->remote_host);

  int r = close(context->fd);
  CHECK(r != 0, "Failed to close client connection");

  context->epoll_info = NULL;
  
  free(context);
}
