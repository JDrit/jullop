#ifndef __request_context_h__
#define __request_context_h__

struct RequestContext;
typedef struct RequestContext RequestContext;

enum Read_State {
  NO_MORE_DATA,
  READ_ERROR,
  CLIENT_DISCONNECT
};

enum Write_State {
  FINISH,
  BUSY,
  ERROR
};

char* read_state_name(enum Read_State);

RequestContext* init_request_context(int fd, char* host_name);

int rc_get_fd(RequestContext *context);

char* rc_get_remote_host(RequestContext *context);

/**
 * Reads the file descriptor associated with the given context
 * and tries to read as much of the data into the buffer.
 *
 * Returns an enum to indicate the outcome of the read operations.
 */
enum Read_State rc_fill_input_buffer(RequestContext *context);

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
enum Write_State rc_write_output(RequestContext *context);

void destroy_request_context(RequestContext *context);

#endif
