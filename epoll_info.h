#ifndef __epoll_info_h__
#define __epoll_info_h__

#include "request_context.h"

struct Stats {
  /* The number of currently open connections */
  size_t active_connections;
  /* The total number of requests that have been handled cumulatively */
  size_t total_requests_processed;
  /* The total number of bytes that have been read on the event loop */
  size_t bytes_read;
  /* The total number of byes that have been written out on the event loop */
  size_t bytes_written;
};

typedef struct EpollInfo {
  /* The file descriptor to use to listen for new connections */
  int accept_fd;
  /* The file descriptor that is used to run the epoll event loop */
  int epoll_fd;
  /* server wide stats */
  struct Stats stats;
} EpollInfo;

/**
 * Creates epoll event loop. The given `accept_fd` is the file descriptor
 * used to listen to new requests on.
 */
EpollInfo *init_epoll_info(int accept_fd);

/**
 * Registers the accept file descriptor to start accepting new requests.
 */
void set_accept_epoll_event(EpollInfo *epoll);

/**
 * Set the request context to start listening for input requests.
 */ 
void input_epoll_event(EpollInfo *epoll, RequestContext *request_context);

/**
 * Sets the request context to change from listining to input requests
 * to start responding to output requests.
 */
void output_epoll_event(EpollInfo *epoll, RequestContext *request_context);

/**
 * Removes the request context from the epoll event loop.
 */
void delete_epoll_event(EpollInfo *epoll, RequestContext *request_context);

#endif
