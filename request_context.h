#ifndef __request_context_h__
#define __request_context_h__

#include <stdint.h>

#include "http_request.h"
#include "time_stats.h"

enum RequestResult {
  REQUEST_SUCCESS,
  REQUEST_EPOLL_ERROR,
  REQUEST_READ_ERROR,
  REQUEST_CLIENT_ERROR,
  REQUEST_WRITE_ERROR,
};

enum RequestState {
  INIT,
  CLIENT_READ,
  CLIENT_WRITE,
  ACTOR_READ,
  ACTOR_WRITE,
  FINISH
};

typedef struct RequestContext {

  /* the address of the client */
  char *remote_host;
  /* the file descriptor to communicate to the client with */
  int fd;

  /* the actor that processed the request */
  int actor_id;

  /* input buffer from the client */
  char *input_buffer;
  /* the total size that can be read from the client */
  size_t input_len;
  /* how much has been read from the client so far */
  size_t input_offset;

  /* the parsed HTTP request for this request */
  HttpRequest http_request;

  /* points to the data that will be written out as the response to
   * the client */
  char *output_buffer;
  /* indicates how long the total response buffer is */
  size_t output_len;
  /* indicates how much data has already been written out to 
   * the client */
  size_t output_offset;
  
  uint8_t flags;

  enum RequestState state;

  /* used to store the time spent on the request */
  TimeStats time_stats;
  
} RequestContext;

/**
 * Constructs a RequestContext to listen handle a client's request on
 * given file descriptor. The host_name is the name of the remote host
 * for the given client.
 */
RequestContext *init_request_context(int fd, char* host_name);

/**
 * The number of bytes read as input from the client.
 */
size_t context_bytes_read(RequestContext *context);

/**
 * The number of bytes written back to the client.
 */
size_t context_bytes_written(RequestContext *context);

/** 
 * Returns 1 when the connection should be kept alive and 0 if the socket
 * should be closed after the response.
 */
int context_keep_alive(RequestContext *context);

/**
 * Generates info-level output of the request.
 */
void context_print_finish(RequestContext *context, enum RequestResult result);

void context_finalize_reset(RequestContext *context, enum RequestResult result);

void context_finalize_destroy(RequestContext *context, enum RequestResult result);

#endif
