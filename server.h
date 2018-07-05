#ifndef __server_h__
#define __server_h__

#include "request_context.h"

typedef struct ActorInfo {
  /* unique identifier for the given actor */
  int id;
  /* the thread ID that the given actor is running on */
  pthread_t pthread_fd;
  /* the file descriptor that the event loop uses to communicate
   * to this actor */
  int selector_fd;
  /* the file descriptor that the actor uses to communicate to
   * the event loop */
  int actor_fd;
} ActorInfo;

typedef struct Server {
  /* the amount of actor running on the server */
  int actor_count;
  /* the list of the actors running */
  ActorInfo *actors;
} Server;

#endif
