#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "actor.h"
#include "http_request.h"
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

HttpRequest *read_http_request(int fd) {
  char buf[sizeof(size_t)];
  size_t offset = 0;

  while (offset < sizeof(size_t)) {
    void *start_addr = buf + offset;
    size_t num_to_read = sizeof(size_t) - offset;
    
    ssize_t num_read = read(fd, start_addr, num_to_read);
    if (num_read == -1) {
      LOG_WARN("Issue reading from selector");
    } else {
      offset += num_to_read;
    }
  }
  HttpRequest *ptr;
  memcpy(&ptr, buf, sizeof(size_t));
  return ptr;
}

void *run_actor(void *pthread_input) {
    
  ActorArgs *args = (ActorArgs*) pthread_input;
  LOG_INFO("Starting actor #%d on fd %d", args->id, args->fd);

  while (1) {
    HttpRequest *request = read_http_request(args->fd);
    LOG_INFO("Received request for %.*s", (int) request->path_len, request->path);   
  }
  
  return NULL;
}


