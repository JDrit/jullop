#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "actor.h"
#include "actor_request.h"
#include "actor_response.h"
#include "http_request.h"
#include "http_response.h"
#include "logging.h"

struct ActorArgs {
  int id;
  int fd;
};

ActorArgs *init_actor_args(int id, int fd) {
  ActorArgs *args = (ActorArgs*) CHECK_MEM(calloc(1, sizeof(ActorArgs)));
  args->id = id;
  args->fd = fd;
  return args;
}

/**
 * Takes the given buffer and passes back to the selector thread the
 * address of the buffer given.
 */
void write_http_response(int fd, ActorResponse *response) {
  size_t offset = 0;
  size_t size = sizeof(ActorResponse);
  
  while (offset < size) {
    void *start_addr = response + offset;
    size_t num_to_write = size - offset;
    ssize_t num_written = write(fd, &start_addr, num_to_write);
    if (num_written == -1) {
      LOG_WARN("Issue writing response back");
    } else {
      offset += (size_t) num_written;
    }
  }
  LOG_DEBUG("Wrote actor response");
}

/**
 * Reads an address of a HTTP request off of the socket and passes back
 * the reference to the caller.
 */
ActorRequest *read_http_request(int fd) {
  char buf[sizeof(size_t)];
  size_t offset = 0;

  while (offset < sizeof(size_t)) {
    void *start_addr = buf + offset;
    size_t num_to_read = sizeof(size_t) - offset;
    
    ssize_t num_read = read(fd, start_addr, num_to_read);
    if (num_read == -1) {
      LOG_WARN("Issue reading from selector");
    } else {
      offset += (size_t) num_read;
    }
  }
  ActorRequest *ptr;
  memcpy(&ptr, buf, sizeof(size_t));
  LOG_DEBUG("Read actor request");
  return ptr;
}

void *run_actor(void *pthread_input) {
    
  ActorArgs *args = (ActorArgs*) pthread_input;
  LOG_INFO("Starting actor #%d on fd %d", args->id, args->fd);

  while (1) {
    ActorRequest *request = read_http_request(args->fd);
    HttpRequest *http_request = request->http_request;
    LOG_INFO("actor %d received request: %.*s %.*s",
	     args->id,
	     (int) http_request->method_len, http_request->method,
	     (int) http_request->path_len, http_request->path);
  
    char *http_response = init_http_response(500,
					     http_request->path,
					     http_request->path_len);
    size_t size = strlen(http_response);
    
    ActorResponse *response = init_actor_response(http_response, size);

    write_http_response(args->fd, response);
  }
  
  return NULL;
}


