
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

RequestContext *init_request_context(int fd, char* host_name) {
  RequestContext *context =
    (RequestContext*) CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->remote_host = host_name;
  context->fd = fd;
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

static void print_request_stats(RequestContext *context, enum RequestResult result) {
  time_t total_request = request_get_total_time(&context->time_stats);
  time_t client_read = request_get_client_read_time(&context->time_stats);
  time_t client_write = request_get_client_write_time(&context->time_stats);
  time_t actor_time = request_get_actor_time(&context->time_stats);
  
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

  // close the connection to the client
  close(context->fd); 

  // free allocated memory for the request
  free(context->remote_host);
  free(context->input_buffer);

  free(context);
}
