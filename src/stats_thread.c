#define _GNU_SOURCE

#include <unistd.h>

#include "logging.h"
#include "queue.h"
#include "server.h"
#include "stats.h"

static size_t queue_usage(Server *server) {
  size_t size = 0;
  for (int actor_id = 0 ; actor_id < server->actor_count ; actor_id++) {
    for (int queue_id = 0 ; queue_id < server->io_worker_count ; queue_id++) {
      size += queue_size(server->app_actors[actor_id].input_queue[queue_id]);
    }
  }
  return size;
}

void *stats_loop(void *pthread_input) {
  Server *server = (Server*) pthread_input;

  while (1) {
    sleep(5);

    LOG_INFO("-------------------------------------------------------");
    LOG_INFO("Stats: total: %lu active: %lu queue: %lu",
	     stats_get_total(server->stats),
	     stats_get_active(server->stats),
	     queue_usage(server));
  }

  return NULL;
}
