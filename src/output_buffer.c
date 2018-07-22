#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "logging.h"
#include "output_buffer.h"

/** 
 * Checks to see if the buffer needs to be resized and does so accordingly.
 * It resizes by pow(n, 2).
 */
static inline void resize(OutputBuffer *buffer) {
  if (buffer->write_into_offset == buffer->length) {
    buffer->length <<= 1;
    buffer->buffer = (char*) CHECK_MEM(realloc(buffer->buffer, buffer->length));
  }
}


OutputBuffer *output_buffer_init(size_t size) {
  OutputBuffer *buffer = (OutputBuffer*) CHECK_MEM(malloc(sizeof(OutputBuffer)));
  buffer->buffer = (char*) CHECK_MEM(malloc(size * sizeof(char)));
  buffer->length = size;
  buffer->write_from_offset = 0;
  buffer->write_into_offset = 0;
  return buffer;
}

void output_buffer_destroy(OutputBuffer *buffer) {
  free(buffer->buffer);
  free(buffer);
}

void output_buffer_reset(OutputBuffer *buffer) {
  buffer->write_from_offset = 0;
  buffer->write_into_offset = 0;
}

enum WriteState output_buffer_write_to(OutputBuffer *buffer, int fd) {
  while (buffer->write_from_offset < buffer->length) {
    void *start_addr = buffer->buffer + buffer->write_from_offset;
    size_t num_to_write = buffer->length - buffer->write_from_offset;
    ssize_t bytes_written = write(fd, start_addr, num_to_write);
    
    switch (bytes_written) {
    case -1:
      if (ERROR_BLOCK) {
	return WRITE_BUSY;
      } else {
	return WRITE_ERROR;
      }
    case 0:
      return WRITE_ERROR;
    default:
      buffer->write_from_offset += (size_t) bytes_written;
      break;
    }
  }
  return WRITE_FINISH;
}

void output_buffer_append(OutputBuffer *buffer, const char *fmt, ...) {
  va_list args;
  va_start (args, fmt);

  while (1) {
    char *start_addr = buffer->buffer + buffer->write_into_offset;
    size_t max_to_write = buffer->length - buffer->write_into_offset;
    int written = vsnprintf(start_addr, max_to_write, fmt, args);
    
    if (max_to_write == (size_t) written) {
      resize(buffer);
    } else {
      buffer->write_into_offset += (size_t) written;
      break;
    }
  }
}
