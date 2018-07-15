#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include "actor.h"
#include "io_worker.h"
#include "logging.h"
#include "queue.h"
#include "request_context.h"
#include "server.h"
#include "queue.h"

static int create_socket(uint16_t port, int queue_length) {
  int opt = 1;
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  CHECK(sock == -1, "Failed to get socket");

  // allows reusing of the same address and port
  CHECK(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &opt, sizeof(opt)) == -1,
	"Failed to set options on SO_REUSEPORT | SO_REUSEADDR");
  // all writes should send packets right away
  CHECK(setsockopt(sock, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt)) == -1,
  	"Failed to set options on TCP_NODELAY");

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  CHECK(bind(sock, (struct sockaddr*) &address, sizeof(address)) == -1,
	"bind failure to %d", port);
  LOG_DEBUG("socket bind complete");
  CHECK(listen(sock, queue_length), "socket listen");
  LOG_DEBUG("listening on %d", port);
  
  return sock;
}

void create_actor(Server *server, int id, ActorInfo *actor) {
  actor->id = id;
  actor->startup = &server->startup;

  actor->input_queue = queue_init(100);
  actor->output_queue = queue_init(100);

  int fds[2];
  int r = socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, fds);
  CHECK(r != 0, "Failed to create actor socket pair");

  actor->input_actor_fd = fds[0];
  actor->actor_requests_fd = fds[1];
  
  // sets the input-actor side file descriptor as non-blocking
  r = fcntl(actor->input_actor_fd, F_SETFL, O_NONBLOCK);
  CHECK(r != 0, "Failed to set non-blocking");

  r = socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, fds);
  CHECK(r != 0, "Failed to create actor socket pair");

  actor->actor_responses_fd = fds[0];
  actor->output_actor_fd = fds[1];

  r = fcntl(actor->output_actor_fd, F_SETFL, O_NONBLOCK);
  CHECK(r != 0, "Failed to set non-blocking");

  pthread_t thread_id;
  r = pthread_create(&thread_id, NULL, run_actor, actor);
  CHECK(r != 0, "Failed to create pthread %d", id);

  r = pthread_detach(thread_id);
  CHECK(r != 0, "Failed to detach pthread");

  size_t max_name_size = 16;
  char *name = (char*) CHECK_MEM(calloc(1, sizeof(max_name_size)));
  snprintf(name, max_name_size, "app-%d", actor->id);
  r = pthread_setname_np(thread_id, name);
  CHECK(r != 0, "Failed to set actor %d name", actor->id);
  
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(id, &cpu_set);
  r = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpu_set);
  CHECK(r != 0, "Failed to set cpu infinity to %d", id);
}

pthread_t create_input_actor(int id, int sock_fd, Server *server) {
  IoWorkerArgs *args = (IoWorkerArgs*) CHECK_MEM(calloc(1, sizeof(IoWorkerArgs)));
  args->id = id;
  args->sock_fd = sock_fd;
  args->server = server;
  
  pthread_t thread;
  int r = pthread_create(&thread, NULL, io_event_loop, args);
  CHECK(r != 0, "Failed to create input actor thread");

  // input actor thread is not detached so that we can call
  // pthread_join on it and block the main thread.

  size_t max_name_size = 16;
  char *name = (char*) CHECK_MEM(calloc(1, sizeof(max_name_size)));
  snprintf(name, max_name_size, "io-%d", args->id);
  r = pthread_setname_np(thread, name);
  CHECK(r != 0, "Failed to set input actor name");

  return thread;
}

int main(int argc, char* argv[]) {
  uint16_t port = 8080;
  int queue_length = 10;
  int io_worker_count = 1;
  int cores = get_nprocs();
  pid_t pid = getpid();
  
  LOG_INFO("starting up pid=%d port=%d", pid, port);


  struct Server server;
  server.io_worker_count = io_worker_count;
  server.actor_count = cores;
  server.app_actors = (ActorInfo*) CHECK_MEM(calloc((size_t) cores, sizeof(ActorInfo)));

  int r = pthread_barrier_init(&server.startup, NULL,
			       (uint) (server.actor_count + server.io_worker_count));
  CHECK(r != 0, "Failed to construct pthread barrier");

  for (int i = 0 ; i < cores ; i++) {
    create_actor(&server, i, &server.app_actors[i]);
  }

  pthread_t input_thread;
  for (int i = 0 ; i < io_worker_count ; i++) {
    int sock_fd = create_socket(port, queue_length);
    input_thread = create_input_actor(i, sock_fd, &server);
  }
  
  void *ptr;
  r = pthread_join(input_thread, (void**) &ptr);
  CHECK(r != 0, "Failed to join on thread");

  LOG_INFO("Exiting server");
}
