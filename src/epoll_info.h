#ifndef __epoll_info_h__
#define __epoll_info_h__

#include <stddef.h>
#include <sys/epoll.h>

typedef struct EpollInfo {
  /* The file descriptor that is used to run the epoll event loop */
  int epoll_fd;
  /* a name to give to the epoll event loop for helpful messages */
  const char *name;

  int id;
  
} EpollInfo;

/**
 * Creates epoll event loop. The given `accept_fd` is the file descriptor
 * used to listen to new requests on.
 */
EpollInfo *epoll_info_init(const char *name, int id);

void epoll_info_destroy(EpollInfo *epoll_info);

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
