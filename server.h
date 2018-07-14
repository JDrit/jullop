#ifndef __server_h__
#define __server_h__

#include "queue.h"

typedef struct ActorInfo {
  /* unique identifier for the given actor. */
  int id;

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
  
} ActorInfo;

typedef struct Server {
  /* the socket to accept input requests on */
  int sock_fd;
  /* the amount of actor running on the server */
  int actor_count;
  /* the list of the actors running */
  ActorInfo *app_actors;
  
} Server;

#endif
