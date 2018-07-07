
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "actor_request.h"
#include "actor_response.h"
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

inline static char* read_state_name(enum ReadState state) {
  switch (state) {
  case READ_FINISH:
    return "READ_FINISH";
  case READ_BUSY:
    return "READ_BUSY";
  case READ_ERROR:
    return "READ_ERROR";
  case CLIENT_DISCONNECT:
    return "CLIENT_DISCONNECT";
  default:
    return "UNKNOWN";
  }
}

RequestContext *init_request_context(int fd, char* host_name) {
  RequestContext *context =
    (RequestContext*) CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->remote_host = host_name;
  context->fd = fd;
  context->input_buffer = (char*) CHECK_MEM(calloc(BUF_SIZE, sizeof(char)));
  context->http_request.num_headers = NUM_HEADERS;
  clock_gettime(CLOCK_MONOTONIC, &context->start_time);
  context->state = CLIENT_READ;
  return context;
}

size_t context_bytes_read(RequestContext *context) {
  return context->input_offset;
}

size_t context_bytes_written(RequestContext *context) {
  return context->output_offset;
}

void context_set_actor(RequestContext *context, ActorInfo *actor) {
  context->actor_info = actor;
  set_flag(context, ACTOR_SET);
}

ActorInfo *context_get_actor(RequestContext *context) {
  assert(!is_flag_set(context, ACTOR_SET));
  return context->actor_info;
}

HttpRequest *context_get_http_request(RequestContext* context) {
  assert (!is_flag_set(context, HTTP_REQUEST_INITIALIZED));
  return &context->http_request;
}

enum WriteState context_write_actor_request(RequestContext *context) {
  assert(context->state == ACTOR_WRITE);
  assert(!is_flag_set(context, ACTOR_SET));
  
  do {
    void* start_addr = &context->actor_request + context->actor_input_offset;
    size_t num_to_write = sizeof(size_t) - context->actor_input_offset;
    ssize_t num_written = write(context->actor_info->selector_fd,
				&start_addr,
				num_to_write);

    if (num_written <= 0) {
      if (errno == EAGAIN) {
	// resets the error condition since it is being handled
	errno = 0;
	return WRITE_BUSY;
      } else {
	return WRITE_ERROR;
      }
    } else {
      context->actor_input_offset += (size_t) num_written;
    }
  } while (context->actor_input_offset < sizeof(size_t));

  return WRITE_FINISH;
}

enum ReadState context_read_actor_response(RequestContext *context) {
  assert (context->state == ACTOR_READ);

  do {
    void *start_addr = context->actor_response_buffer + context->actor_output_offset;
    size_t num_to_read = sizeof(size_t) - context->actor_output_offset;
    ssize_t num_read = read(context->actor_info->selector_fd,
			    start_addr,
			    num_to_read);
    if (num_read <= 0) {
      if (errno == EAGAIN) {
	errno = 0;
	return READ_BUSY;
      } else {
	return READ_ERROR;
      }
    } else {
      context->actor_output_offset += (size_t) num_read;
    }
  } while (context->actor_output_offset < sizeof(size_t));

  memcpy(&context->actor_response,
	 context->actor_response_buffer,
	 sizeof(size_t));
  return READ_FINISH;
}

enum WriteState context_write_client_response(RequestContext *context) {
  assert (context->state == CLIENT_WRITE);

  do {

    void* start_addr = context->actor_response->buffer + context->output_offset;
    size_t num_to_write = context->actor_response->length - context->output_offset;
    ssize_t num_written = write(context->fd, start_addr, num_to_write);
    LOG_DEBUG("Wrote %zd bytes to %s", num_written, context->remote_host);

    if (num_written <= 0) {
      if (errno == EAGAIN) {
	// resets the error condition since it is being handled.
	errno = 0;
	return WRITE_BUSY;
      } else {
	return WRITE_ERROR;
      }
    } else {
      context->output_offset += (size_t) num_written;
    }
  } while (context->output_offset < context->actor_response->length);

  return WRITE_FINISH;
}

enum ReadState context_read_client_request(RequestContext *context) {
  assert (context->state == CLIENT_READ);

  while (1) {
    
    void* start_addr = context->input_buffer + context->input_offset;
    size_t max_to_read = BUF_SIZE - context->input_offset;
    ssize_t num_read = read(context->fd, start_addr, max_to_read);
    LOG_DEBUG("Read %zd bytes from %s", num_read, context->remote_host);

    if (num_read == -1) {
      if (errno == EAGAIN) {
	// resets the error condition since it is being handled.
	errno = 0;
	return READ_BUSY;
      } else {
	return READ_ERROR;
      }
    } else if (num_read == 0) { 
      return CLIENT_DISCONNECT;
    } else {
      size_t prev_len = context->input_offset;
      context->input_offset += (size_t) num_read;				   
      enum ParseState p_state = http_parse(context->input_buffer,
					   context->input_offset,
					   prev_len,
					   &context->http_request);
      switch (p_state) {
      case PARSE_FINISH:
	set_flag(context, HTTP_REQUEST_INITIALIZED);
	// initilizes the actor request
	context->actor_request.http_request = &context->http_request;	
	return READ_FINISH;
      case PARSE_ERROR:
	return READ_ERROR;
      case PARSE_INCOMPLETE:
	return READ_BUSY;
      }

    }
  }
}

static void print_request_stats(RequestContext *context, enum RequestResult result) {
  time_t total_request = get_total_request_time(&context->time_stats);
  time_t client_read = get_client_read_time(&context->time_stats);
  time_t client_write = get_client_write_time(&context->time_stats);
  time_t actor_time = get_actor_time(&context->time_stats);
  
  LOG_INFO("Request Stats:");
  LOG_INFO("  result=%s", request_result_name(result));
  LOG_INFO("  remote_host=%s", context->remote_host);
  LOG_INFO("  bytes_read=%zu", context->input_offset);
  LOG_INFO("  bytes_written=%zu", context->output_offset);
  LOG_INFO("Time Stats:");
  LOG_INFO("  total_micros=%ld", total_request);
  LOG_INFO("  client_read_micros=%ld", client_read);
  LOG_INFO("  actor_micros=%ld", actor_time);
  LOG_INFO("  client_write_micros=%ld", client_write);
}

void request_finish_destroy(RequestContext *context, enum RequestResult result) {
  print_request_stats(context, result);
  close(context->fd);
  free(context->remote_host);
  free(context->input_buffer);

  if (context->actor_response != NULL) {
    free(context->actor_response);
  }
  
  free(context);
}
