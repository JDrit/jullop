#ifndef __db_event_loop__
#define __db_event_loop__

#include "server.h"

void event_loop(Server *server, int sock_fd);

#endif
