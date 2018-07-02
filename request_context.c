
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "request_context.h"
#include "logging.h"
#include "http_request.h"

#define BUF_SIZE 2048

enum Flags {
  HTTP_REQUEST_INITIALIZED = 1 << 0,
  OUTPUT_BUFFER_SET = 1 << 1,
};

struct RequestContext {
  char *remote_host;
  int fd;
  
  char *input_buffer;
  size_t input_offset;

  char *output_buffer;
  size_t output_size;
  size_t output_offset;

  HttpRequest *http_request;

  uint8_t flags;

  struct timespec start_time;
};

/* returns 0 if flag is set, -1 otherwise */
int is_flag_set(RequestContext *context, enum Flags flag) {
  if ((context->flags & flag) == 0) {
    return -1;
  } else {
    return 0;
  }
}

void set_flag(RequestContext *context, enum Flags flag) {
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

RequestContext* init_request_context(int fd, char* host_name) {
  RequestContext *context =
    (RequestContext*) CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->remote_host = host_name;
  context->fd = fd;
  context->input_buffer = (char*) CHECK_MEM(calloc(BUF_SIZE, sizeof(char)));
  context->http_request = (HttpRequest*) CHECK_MEM(calloc(1, sizeof(HttpRequest)));
  context->http_request->num_headers =
    sizeof(context->http_request->headers) / sizeof(context->http_request->headers[0]);
  clock_gettime(CLOCK_MONOTONIC, &context->start_time);
  return context;
}

int rc_get_fd(RequestContext *context) {
  return context->fd;
}

char* rc_get_remote_host(RequestContext *context) {
  return context->remote_host;
}

size_t rc_get_bytes_read(RequestContext *context) {
  return context->input_offset;
}

size_t rc_get_bytes_written(RequestContext *context) {
  return context->output_offset;
}

HttpRequest* rc_get_http_request(RequestContext* context) {
  assert (!is_flag_set(context, HTTP_REQUEST_INITIALIZED));
  return context->http_request;
}

enum ReadState rc_fill_input_buffer(RequestContext *context) {
  ssize_t num_read;
  while (1) {
    void* start_addr = context->input_buffer + context->input_offset;
    size_t max_to_read = BUF_SIZE - context->input_offset;
    num_read = read(context->fd, start_addr, max_to_read);
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
					   context->http_request);

      switch (p_state) {
      case PARSE_FINISH:
	set_flag(context, HTTP_REQUEST_INITIALIZED);
	return READ_FINISH;
      case PARSE_ERROR:
	return READ_ERROR;
      case PARSE_INCOMPLETE:
	return READ_BUSY;
      }

    }
  }
}

enum WriteState rc_write_output(RequestContext *context) {
  assert (!is_flag_set(context, OUTPUT_BUFFER_SET));
  
  while (1) {
    void* start_addr = context->output_buffer + context->output_offset;
    size_t num_to_write = context->output_size - context->output_offset;
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
      if (context->output_offset == context->output_size) {
	return WRITE_FINISH;
      }
    }
  }
}

void rc_set_output_buffer(RequestContext *context, char* output, size_t output_size) {
  context->output_buffer = output;
  context->output_size = output_size;
  set_flag(context, OUTPUT_BUFFER_SET);
}

void print_request_stats(RequestContext *context, enum RequestResult result) {
  struct timespec end_time;
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  struct timespec diff_time;
  diff_time.tv_sec = end_time.tv_sec - context->start_time.tv_sec;
  diff_time.tv_nsec = end_time.tv_nsec - context->start_time.tv_nsec;
  time_t microseconds = 1000000 * diff_time.tv_sec + diff_time.tv_nsec / 1000;
  
  LOG_INFO("Request Stats");
  LOG_INFO("\tresult=%s", request_result_name(result));
  LOG_INFO("\tremote_host=%s", context->remote_host);
  LOG_INFO("\tbytes_read=%zu", context->input_offset);
  LOG_INFO("\tbytes_written=%zu", context->output_size);
  LOG_INFO("\tmicroseconds=%ld", microseconds);

}
void rc_finish_destroy(RequestContext *context, enum RequestResult result) {
  print_request_stats(context, result);
  close(context->fd);
  free(context->remote_host);
  free(context->input_buffer);
  if (context->output_buffer != NULL) {
    free(context->output_buffer);
  }
  free(context);
}
