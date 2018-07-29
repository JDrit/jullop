#ifndef __input_actor_h__
#define __input_actor_h__

#include "epoll_info.h"
#include "request_context.h"
#include "server.h"

typedef struct SocketContext {
  /* called when epoll detects the given file descriptor has data
   * ready to be read. */
  void (*input_handler) (struct SocketContext *context);

  /* called when epoll detects the given file descriptor is 
   * ready to be written to. */
  void (*output_handler) (struct SocketContext *context);

  /* called when epoll detects an error on the given file
   * descriptor. */
  void (*error_handler) (struct SocketContext *context, uint32_t events);


  union data {
    int fd;
    void *ptr;
  } data;
  
  Server *server;
  EpollInfo *epoll_info;  
} SocketContext;

typedef struct IoWorkerArgs {
  /* unique ID of the worker */
  int id;
  /* the socket that the worker should listen for connections on */
  int sock_fd;
  /* reference to the server config */
  Server *server;
} IoWorkerArgs;

SocketContext *init_context(Server *server, EpollInfo *epoll_info);

/**
 * Runs the event loop in the current thread to process
 * requests off the specified socket.
 */
void *io_event_loop(void *pthread_input);

#endif
