#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "actor.h"
#include "logging.h"

#define ACTOR_BUF_SIZE 256

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

void *run_actor(void *pthread_input) {
    
  ActorArgs *args = (ActorArgs*) pthread_input;
  LOG_INFO("Starting actor #%d on fd %d", args->id, args->fd);
  void *buf[ACTOR_BUF_SIZE];

  while (1) {
    ssize_t num_read = recv(args->fd, buf, ACTOR_BUF_SIZE, 0);

    if (num_read == -1) {
	LOG_WARN("Issue reading from selector");
    } else {
      LOG_INFO("Actor received request");
    }
    
  }
  
  return NULL;
}


