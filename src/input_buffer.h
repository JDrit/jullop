#ifndef __input_buffer_h__
#define __input_buffer_h__

enum ReadState {
  READ_FINISH,
  READ_BUSY,
  READ_ERROR,
  CLIENT_DISCONNECT
};

typedef struct InputBuffer {
  char *buffer;
  size_t length;
  size_t offset;
} InputBuffer;

/**
 * Constructs a resizable buffer to read and write from a file descriptor
 * with an initial size of the argument.
 */
InputBuffer *input_buffer_init(size_t initial_size);

/**
 * Resets the buffer so that it can be used again.
 */
void input_buffer_reset(InputBuffer *buffer);

/**
 * Cleans up all memory associated with this buffer.
 */
void input_buffer_destroy(InputBuffer *buffer);

/**
 * Reads as much as possible into the input buffer from the socket
 * associated with the given file descriptor.
 */
enum ReadState input_buffer_read_into(InputBuffer *buffer, int fd);

#endif
