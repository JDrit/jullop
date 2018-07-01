#ifndef __request_context_h__
#define __request_context_h__

#include "http_request.h"

struct RequestContext;
typedef struct RequestContext RequestContext;

enum ReadState {
  READ_FINISH,
  READ_BUSY,
  READ_ERROR,
  CLIENT_DISCONNECT
};

enum WriteState {
  WRITE_FINISH,
  WRITE_BUSY,
  WRITE_ERROR
};

enum RequestResult {
  REQUEST_SUCCESS,
  REQUEST_EPOLL_ERROR,
  REQUEST_READ_ERROR,
  REQUEST_CLIENT_ERROR,
  REQUEST_WRITE_ERROR,
};

RequestContext* init_request_context(int fd, char* host_name);

int rc_get_fd(RequestContext *context);

char* rc_get_remote_host(RequestContext *context);

size_t rc_get_bytes_read(RequestContext *context);

size_t rc_get_bytes_written(RequestContext *context);

/**
 * Gets the underlying HTTP request that this context has read. This method
 * cannot be called till the rc_fill_input_buffer has returned a ReadState
 * indicating the request has be successfully read.
 */
HttpRequest* rc_get_http_request(RequestContext *context);

/**
 * Reads the file descriptor associated with the given context
 * and tries to read as much of the data into the buffer.
 *
 * Returns an enum to indicate the outcome of the read operations.
 */
enum ReadState rc_fill_input_buffer(RequestContext *context);

/**
 * Sets the buffer that will be sent to the client.
 */
void rc_set_output_buffer(RequestContext *context, char* buffer, size_t output_size);

/**
 * Attempts to write out as much as possible to the underlying
 * socket. Returns an enum to indicate the output of the write.
 * 
 * There is undefinied behavior when an output buffer has not been set before
 * this has been called.
 */
enum WriteState rc_write_output(RequestContext *context);

/**
 * Indicates that the request has is done and that all resources should be 
 * clean up for it. The RequestResult is used to indicate the final result
 * of the request.
 */
void rc_finish_destroy(RequestContext *context, enum RequestResult result);

#endif
