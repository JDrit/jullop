#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "logging.h"
#include "output_buffer.h"

/** 
 * Checks to see if the buffer needs to be resized and does so accordingly.
 * The new size is either min_size or pow(size, 2), whichever is larger.
 */
static inline void resize(OutputBuffer *buffer, size_t min_size) {
  size_t new_size;

  if ((buffer-> length << 1) > min_size) {
    new_size = buffer->length << 1;
  } else {
    new_size = min_size;
  }
  buffer->length = new_size;
  buffer->buffer = (char*) CHECK_MEM(realloc(buffer->buffer, buffer->length));
  buffer->resize_count++;
}


OutputBuffer *output_buffer_init(size_t size) {
  OutputBuffer *buffer = (OutputBuffer*) CHECK_MEM(malloc(sizeof(OutputBuffer)));
  buffer->buffer = (char*) CHECK_MEM(malloc(size * sizeof(char)));
  buffer->length = size;
  buffer->write_from_offset = 0;
  buffer->write_into_offset = 0;
  buffer->resize_count = 0;
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
  while (buffer->write_from_offset < buffer->write_into_offset) {
    void *start_addr = buffer->buffer + buffer->write_from_offset;
    size_t num_to_write = buffer->write_into_offset - buffer->write_from_offset;
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

  while (1) {
    va_start(args, fmt);
    
    char *start_addr = buffer->buffer + buffer->write_into_offset;
    size_t max_to_write = buffer->length - buffer->write_into_offset;
    int written = vsnprintf(start_addr, max_to_write, fmt, args);
    CHECK(written < 0, "Failed to construct output buffer");

    if ((size_t) written > max_to_write) {
      resize(buffer, buffer->write_into_offset + ((size_t) written) + 1);
    } else {
      buffer->write_into_offset += (size_t) written;
      break;
    }
    va_end(args);
  }
}
