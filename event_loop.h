#ifndef __db_event_loop__
#define __db_event_loop__

#include "server.h"

EventLoop* init_event_loop(int accept_fd);

void event_loop(Server *server);

#endif
