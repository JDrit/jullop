
#include <stdlib.h>
#include <unistd.h>
#include "request_context.h"
#include "logging.h"

#define BUF_SIZE 2048

struct RequestContext {
  char *remote_host;
  int fd;
  
  char *input_buffer;
  size_t input_offset;

  char *output_buffer;
  size_t output_size;
  size_t output_offset;
  
};

char* read_state_name(enum Read_State state) {
  switch (state) {
    case NO_MORE_DATA:
      return "NO_MORE_DATA";
    case READ_ERROR:
      return "READ_ERROR";
    case CLIENT_DISCONNECT:
      return "CLIENT_DISCONNECT";
    default:
      return "UNKNOWN";
  }
}

RequestContext* init_request_context(int fd, char* host_name) {
  struct RequestContext *context = (struct RequestContext*)
    CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->remote_host = host_name;
  context->fd = fd;
  context->input_buffer = (char*) CHECK_MEM(calloc(BUF_SIZE, sizeof(char)));
  context->input_offset = 0;
  context->output_buffer = NULL;
  context->output_size = 0;
  context->output_offset = 0;
  return context;
}

int rc_get_fd(RequestContext *context) {
  return context->fd;
}

char* rc_get_remote_host(RequestContext *context) {
  return context->remote_host;
}

enum Read_State rc_fill_input_buffer(RequestContext *context) {
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
	return NO_MORE_DATA;
      } else {
	return READ_ERROR;
      }
    } else if (num_read == 0) { 
      return CLIENT_DISCONNECT;
    } else {
      context->input_offset += (size_t) num_read;
    }
  }
}

enum Write_State rc_write_output(RequestContext *context) {
  while (1) {
    void* start_addr = context->output_buffer + context->output_offset;
    size_t num_to_write = context->output_size - context->output_offset;
    ssize_t num_written = write(context->fd, start_addr, num_to_write);
    LOG_DEBUG("Wrote %zd bytes to %s", num_written, context->remote_host);

    if (num_written <= 0) {
      if (errno == EAGAIN) {
	// resets the error condition since it is being handled.
	errno = 0;
	return BUSY;
      } else {
	return ERROR;
      }
    } else {
      context->output_offset += (size_t) num_written;
      if (context->output_offset == context->output_size) {
	return FINISH;
      }
    }
  }
}

void rc_set_output_buffer(RequestContext *context, char* output, size_t output_size) {
  context->output_buffer = output;
  context->output_size = output_size;
}

void destroy_request_context(RequestContext *context) {
  free(context->remote_host);
  free(context->input_buffer);
  if (context->output_buffer != NULL) {
    free(context->output_buffer);
  }
  free(context);
}
