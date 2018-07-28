#ifndef __input_actor_h__
#define __input_actor_h__

#include <stdatomic.h>

#include "request_context.h"
#include "server.h"

typedef struct IoWorkerStats {
    /* The number of currently open connections */
  _Atomic size_t active_connections;

  /* The total number of requests that have been handled cumulatively */
  _Atomic size_t total_requests_processed;

} IoWorkerStats;

typedef struct IoWorkerArgs {
  /* unique ID of the worker */
  int id;
  /* the socket that the worker should listen for connections on */
  int sock_fd;
  /* reference to the server config */
  Server *server;
} IoWorkerArgs;

typedef struct ActorContext {
  /* the id of the actor. */
  int id;
  /* the event file descriptor used for messages from the actor. */
  int fd;
  /* the queue to receieve events back from the actor. */
  Queue *queue;
} ActorContext;

typedef struct AcceptContext {
  /* the file descriptor to listen for new connections on. */
  int fd;
} AcceptContext;

typedef struct IoContext {

  enum type {
    CLIENT,
    ACTOR,
    ACCEPT,
  } type;

  union context {
    RequestContext *request_context;
    ActorContext *actor_context;
    AcceptContext *accept_context;
  } context;
  
} IoContext;

/**
 * Runs the event loop in the current thread to process
 * requests off the specified socket.
 */
void *io_event_loop(void *pthread_input);

#endif
