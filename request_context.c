#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "http_request.h"
#include "logging.h"
#include "request_context.h"
#include "server.h"

#define BUF_SIZE 2048

enum Flags {
  HTTP_REQUEST_INITIALIZED = 1 << 0,
  OUTPUT_BUFFER_SET = 1 << 1,
  ACTOR_SET = 1 << 2,
  HTTP_RESPONSE_INITIALIZED = 1 << 3,
};

/* returns 0 if flag is set, -1 otherwise */
static int is_flag_set(RequestContext *context, enum Flags flag) {
  if ((context->flags & flag) == 0) {
    return -1;
  } else {
    return 0;
  }
}

static void set_flag(RequestContext *context, enum Flags flag) {
  context->flags |= flag;
}

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

RequestContext *init_request_context(int fd, char* host_name) {
  RequestContext *context =
    (RequestContext*) CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->remote_host = host_name;
  context->fd = fd;
  context->actor_id = -1;
  context->input_buffer = (char*) CHECK_MEM(calloc(BUF_SIZE, sizeof(char)));
  context->input_len = BUF_SIZE;
  context->http_request.num_headers = NUM_HEADERS;
  context->state = CLIENT_READ;
  return context;
}

size_t context_bytes_read(RequestContext *context) {
  return context->input_offset;
}

size_t context_bytes_written(RequestContext *context) {
  return context->output_offset;
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

static inline void print_request_stats(RequestContext *context, enum RequestResult result) {  
  LOG_DEBUG("Request Stats : result=%s fd=%d remote_host=%s actor=%d "
	   "bytes_read=%zu bytes_written=%zu",
	    request_result_name(result),
	    context->fd,
	    context->remote_host,
	    context->actor_id,
	    context->input_offset,
	    context->output_offset);
  LOG_DEBUG("Time Stats    : total_micros=%ld client_read_micros=%ld "
	   "actor_micros=%ld client_write_micros=%ld",
	    request_get_total_time(&context->time_stats),
	    request_get_client_read_time(&context->time_stats),
	    request_get_actor_time(&context->time_stats),
	    request_get_client_write_time(&context->time_stats));
}

void context_finalize_reset(RequestContext *context, enum RequestResult result) {
  print_request_stats(context, result);

  context->actor_id = -1;
  
  // reset the input buffer
  context->input_len = 0;
  context->input_offset = 0;

  if (context->output_buffer != NULL) {
    free(context->output_buffer);
  }
  
  // reset the output buffer
  context->output_len = 0;
  context->output_offset = 0;
  context->flags = 0;

  context->state = CLIENT_READ;

  memset(&context->time_stats, 0, sizeof(TimeStats));
}

void context_finalize_destroy(RequestContext *context, enum RequestResult result) {
  print_request_stats(context, result);

  
  // free allocated memory for the request
  free(context->input_buffer);

  // free the buffer for the address of the remote hosr
  free(context->remote_host);

  if (context->output_buffer != NULL) {
    free(context->output_buffer);
  }
  close(context->fd);

  free(context);
}
