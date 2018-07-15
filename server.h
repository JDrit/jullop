#ifndef __server_h__
#define __server_h__

#include <pthread.h>
#include <sys/eventfd.h>

#include "queue.h"

typedef struct ActorInfo {
  /* unique identifier for the given actor. */
  int id;

  Queue *input_queue;

  Queue *output_queue;

  /* the file descriptor that the input actor uses to send requests to
   * the application-level actors. */
  int input_actor_fd;

  /* the file descriptor that the application-level actor uses to 
   * listen for requests on. */
  int actor_requests_fd;

  /* the file descriptor that the actor uses to send back responses. */
  int actor_responses_fd;

  /* the file descriptor that the output actor uses to listen for
   * request on. */
  int output_actor_fd;

  /* this is just a reference to the pthread_barrier_t owned by the
   * server struct. */
  pthread_barrier_t *startup;
  
} ActorInfo;

typedef struct Server {
  int io_worker_count;
  
  /* the amount of actor running on the server */
  int actor_count;
  /* the list of the actors running */
  ActorInfo *app_actors;

  /* used to block all threads till the application actors have
   * started. */
  pthread_barrier_t startup;
  
} Server;

#endif
