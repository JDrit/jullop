#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>

#include "input_buffer.h"
#include "logging.h"

/** 
 * Checks to see if the buffer needs to be resized and does so accordingly.
 * It resizes by pow(n, 2).
 */
static inline void resize(InputBuffer *buffer) {
  if (buffer->offset == buffer->length) {
    buffer->length <<= 1;
    buffer->buffer = (char*) CHECK_MEM(realloc(buffer->buffer, buffer->length));
    LOG_DEBUG("resizing to %zu", buffer->length);
  }
}

InputBuffer *input_buffer_init(size_t size) {
  InputBuffer *buffer = (InputBuffer*) CHECK_MEM(malloc(sizeof(InputBuffer)));
  buffer->buffer = (char*) CHECK_MEM(malloc(size * sizeof(char)));
  buffer->length = size;
  buffer->offset = 0;
  return buffer;
}

void input_buffer_reset(InputBuffer *buffer) {
  buffer->offset = 0;
}

void input_buffer_destroy(InputBuffer *buffer) {
  free(buffer->buffer);
  free(buffer);
}

enum ReadState input_buffer_read_into(InputBuffer *buffer, int fd) {
  while (1) {
    resize(buffer);    errno = 0;

    void *start_addr = buffer->buffer + buffer->offset;
    size_t num_to_read = buffer->length - buffer->offset;
    LOG_DEBUG("num to read %zu ad offset %zu", num_to_read, buffer->offset);
    ssize_t bytes_read = read(fd, start_addr, num_to_read);
    LOG_WARN("bytes read %zd", bytes_read);

    switch (bytes_read) {
    case -1:
      if (ERROR_BLOCK) {
	return READ_BUSY;
      } else {
	return READ_ERROR;
      }
    case 0:
      return READ_ERROR;
    default:
      buffer->offset += (size_t) bytes_read;
      break;
    }
  }
}
