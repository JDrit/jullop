#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include "actor.h"
#include "logging.h"
#include "request_context.h"
#include "event_loop.h"

#define PORT 8080
#define QUEUE_LENGTH 10

struct Server {
  int actor_count;
  pthread_t *actor_pthreads;
  EventLoop *event_loop;
};

int create_socket() {
  int opt = 1;
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  CHECK(sock == -1, "failed to get socket");

  // allows reusing of the same address and port
  CHECK(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &opt, sizeof(opt)) == -1,
	"Failed to set options on SO_REUSEPORT | SO_REUSEADDR");
  // all writes should send packets right away
  CHECK(setsockopt(sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1,
	"Failed to set options on TCP_NODELAY");

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  CHECK(bind(sock, (struct sockaddr*) &address, sizeof(address)) == -1,
	"bind failure to %d", PORT);
  LOG_DEBUG("socket bind complete");
  CHECK(listen(sock, QUEUE_LENGTH), "socket listen");
  LOG_DEBUG("listening on %d", PORT);
  
  return sock;
}

/**
 * Creates an actor thread and assigns it the given id. A socket pair is
 * created and one is given to the actor and the other is returned.
 */
int create_actor(struct Server *server, int id) {
  int fds[2];
  int r = socketpair(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0, fds);
  CHECK(r != 0, "Failed to create actor socket pair");
    
  ActorArgs *args = init_actor_args(id, fds[0]);
  r = pthread_create(&server->actor_pthreads[id], NULL, run_actor, args);
  CHECK(r != 0, "Failed to create pthread %d", id);

  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(id, &cpu_set);
  r = pthread_setaffinity_np(server->actor_pthreads[id], sizeof(cpu_set_t),
			     &cpu_set);
  CHECK(r != 0, "Failed to set cpu infinity to %d", id);
  
  return fds[1];
}

int main(int argc, char* argv[]) {
  LOG_INFO("starting up...");
  int sock_fd = create_socket();
  int cores = get_nprocs();

  struct Server server;
  server.actor_count = cores;
  server.actor_pthreads = (pthread_t*) CHECK_MEM(calloc((size_t) cores, sizeof(pthread_t)));

  int *actor_input_fds = (int*) CHECK_MEM(calloc((size_t) cores, sizeof(int)));
  for (int i = 0 ; i < cores ; i++) {
    actor_input_fds[i] = create_actor(&server, i);
  }
  server.event_loop = init_event_loop(sock_fd, cores, actor_input_fds);

				     
  event_loop(server.event_loop);
}
