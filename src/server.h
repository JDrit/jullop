#ifndef __server_h__
#define __server_h__

#include <pthread.h>

#include "server_stats.h"
#include "queue.h"

typedef struct ActorInfo {
  /* unique identifier for the given actor. */
  int id;

  /* reference to the global server config. */
  struct Server *server;

  /* the number of input / output queues that this actor has. This
   * is equal to the number of IO workers in the system. */
  int queue_count;

  /* All requests that are going to this actor should be put into
   * this queue. */
  Queue **input_queue;

  /* this is just a reference to the pthread_barrier_t owned by the
   * server struct. */
  pthread_barrier_t *startup;
  
} ActorInfo;

typedef struct Server {
  ServerWideStats *server_stats;
  
  /* the number of threads used to read/write client requests. */
  int io_worker_count;
  
  /* the amount of actor running on the server. */
  int actor_count;

  /* the list of the actors running. */
  ActorInfo *app_actors;

  /* used to block all threads till the application actors have
   * started. */
  pthread_barrier_t startup;
  
} Server;

#endif
