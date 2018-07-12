#define _GNU_SOURCE

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "message_passing.h"
#include "request_context.h"

void message_init(Message *message, void *ptr) {
  memcpy(message->buffer, &ptr, sizeof(size_t));
  message->offset = 0;
}

void message_reset(Message *message) {
  message->offset = 0;
}

void *message_get_payload(Message *message) {
  // can only get the payload if the message has been fully read
  assert(message->offset == sizeof(size_t));

  void *ptr;
  memcpy(&ptr, message->buffer, sizeof(size_t));
  return ptr;
}

void message_write_sync(int fd, Message *message) {
  do {
    void *start_addr = message->buffer + message->offset;
    size_t num_to_write = sizeof(size_t) - message->offset;
    ssize_t num_written = write(fd, start_addr, num_to_write);
    
    if (num_written == -1) {
      LOG_WARN("Error during blocking write");
    } else {
      message->offset += (size_t) num_written;
    }
  } while (message->offset < sizeof(size_t));  
  LOG_DEBUG("Finished blocking write on fd=%d", fd);
}

void message_read_sync(int fd, Message *message) {

  do {
    void *start_addr = message->buffer + message->offset;
    size_t num_to_read = sizeof(size_t) - message->offset;
    ssize_t num_read = read(fd, start_addr, num_to_read);
    LOG_DEBUG("read %zd bytes on fd %d", num_read, fd);
    
    if (num_read <= 0) {
      LOG_WARN("Error during blocking read");
    } else {
      message->offset += (size_t) num_read;
    }
  } while (message->offset < sizeof(size_t));
  LOG_DEBUG("Finished blocking read on fd=%d", fd);
}

enum WriteState message_write_async(int fd, Message *message) {
  return write_async(fd, message->buffer, sizeof(size_t), &message->offset);
}

enum ReadState message_read_async(int fd, Message* message) {
  return read_async(fd, message->buffer, sizeof(size_t), &message->offset);
}

enum WriteState write_async(int fd, void *buffer, size_t buffer_len,
			    size_t *offset) {
  do {
    void *start_addr = buffer + *offset;
    size_t num_to_write = buffer_len - *offset;
    ssize_t num_written = write(fd, start_addr, num_to_write);

    switch (num_written) {
    case -1:
      if (ERROR_BLOCK) {
	return WRITE_BUSY;
      } else {
	return WRITE_ERROR;
      }
    case 0:
      return WRITE_ERROR;
    default:
      *offset += (size_t) num_written;
    }
  } while (*offset < buffer_len);
  return WRITE_FINISH;
}

enum ReadState read_async(int fd, void *buffer, size_t buffer_len,
			  size_t *offset) {
  do {
    void *start_addr = buffer + *offset;
    size_t num_to_read = buffer_len - *offset;
    ssize_t num_read = read(fd, start_addr, num_to_read);

    switch (num_read) {
    case -1:
      if (ERROR_BLOCK) {
	return READ_BUSY;
      } else {
	return READ_ERROR;
      }
    case 0:
      return READ_ERROR;
    default:
      *offset += (size_t) num_read;
      break;
    }
  } while (*offset < sizeof(size_t));
  return READ_FINISH;
}
