#ifndef __db_event_loop__
#define __db_event_loop__

struct EventLoop;
typedef struct EventLoop EventLoop;

EventLoop* init_event_loop(int accept_fd, int actor_count, int *actor_fds);

void event_loop(EventLoop *eventLoop);

#endif
