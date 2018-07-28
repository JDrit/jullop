#define _GNU_SOURCE

#include <unistd.h>

#include "logging.h"
#include "queue.h"
#include "server.h"
#include "stats.h"

static void queue_sizes(Server *server, size_t *input_size, size_t *output_size) {
  for (int actor_id = 0 ; actor_id < server->actor_count ; actor_id++) {
    for (int queue_id = 0 ; queue_id < server->io_worker_count ; queue_id++) {
      *input_size += queue_size(server->app_actors[actor_id].input_queue[queue_id]);
      *output_size += queue_size(server->app_actors[actor_id].output_queue[queue_id]);
    }
  }
}

void *stats_loop(void *pthread_input) {
  Server *server = (Server*) pthread_input;

  while (1) {
    sleep(5);

    size_t input_size = 0;
    size_t output_size = 0;
    queue_sizes(server, &input_size, &output_size);
    
    LOG_INFO("-------------------------------------------------------");
    LOG_INFO("Stats: total: %lu active: %lu",
	     stats_get_total(server->stats),
	     stats_get_active(server->stats));
    LOG_INFO("Actor: input: %lu output: %lu", input_size, output_size);
  }

  return NULL;
}
