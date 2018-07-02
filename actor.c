#include <stdlib.h>
#include "actor.h"
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

void *run_actor(void *pthread_input) {
  ActorArgs *args = (ActorArgs*) pthread_input;
  LOG_INFO("Starting actor #%d on fd %d", args->id, args->fd);
  
  return NULL;
}


