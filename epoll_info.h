#ifndef __epoll_info_h__
#define __epoll_info_h__

#include <stddef.h>
#include <sys/epoll.h>

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
  /* The file descriptor that is used to run the epoll event loop */
  int epoll_fd;
  /* server wide stats */
  struct Stats stats;
} EpollInfo;

enum EpollError {
  NONE,
  EPOLL_ERROR,
};

/**
 * Creates epoll event loop. The given `accept_fd` is the file descriptor
 * used to listen to new requests on.
 */
EpollInfo *init_epoll_info();

/**
 * Registers the accept file descriptor to start accepting new requests.
 * The given EpollInfo is registered as the event context.
 */
void set_accept_epoll_event(EpollInfo *epoll, int fd);

/**
 * Checks to see if the given epoll event contains an error.
 */
enum EpollError check_epoll_errors(EpollInfo *epoll, struct epoll_event *event);

/**
 * Registers the given file descriptor to start accepting input. Adds the 
 * given pointer to the event.
 */ 
void add_input_epoll_event(EpollInfo *epoll, int fd, void *ptr);

/**
 * Registers the given file descriptor to start accepting write requests. 
 * Adds the given pointer to the event.
 */
void add_output_epoll_event(EpollInfo *epoll, int fd, void *ptr);

/**
 * Changes the given file descriptor to start accepting read requests.
 * Adds the given pointer to the event context.
 */
void mod_input_epoll_event(EpollInfo *epoll, int fd, void *ptr);

/**
 * Changes the given file descriptor to start accepting write requests.
 * Adds the given pointer to the event context.
 */
void mod_output_epoll_event(EpollInfo *epoll, int fd, void *ptr);

/**
 * Removes the file descriptor from the epoll event loop.
 */
void delete_epoll_event(EpollInfo *epoll, int fd);

#endif
