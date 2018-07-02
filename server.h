#ifndef __server_h__
#define __server_h__

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

typedef struct EventLoop {
  /* The file descriptor to use to listen for new connections */
  int accept_fd;
  /* The file descriptor that is used to run the epoll event loop */
  int epoll_fd;
  /* Stores stats about the overall system */
  struct Stats stats;
} EventLoop;


typedef struct ActorInfo {
  int id;
  pthread_t pthread_fd;
  int selector_fd;
  int actor_fd;
} ActorInfo;

typedef struct Server {
  int actor_count;
  ActorInfo *actors;
  EventLoop *event_loop;
} Server;

#endif
