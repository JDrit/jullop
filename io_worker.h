#ifndef __input_actor_h__
#define __input_actor_h__

#include "server.h"

typedef struct IoWorkerArgs {
  /* unique ID of the worker */
  int id;
  /* the socket that the worker should listen for connections on */
  int sock_fd;
  /* reference to the server config */
  Server *server;
} IoWorkerArgs;


void *io_event_loop(void *pthread_input);

#endif
