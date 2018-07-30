#define _GNU_SOURCE

#include <locale.h>
#include <unistd.h>

#include "logging.h"
#include "queue.h"
#include "request_stats.h"
#include "server.h"
#include "server_stats.h"

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
    setlocale(LC_NUMERIC, "");
    LOG_INFO("-------------------------------------------------------\n"
	     "Stats : total requests: %'lu active requests: %'lu queue size: %lu\n"
             "Time  : total: %'.0lfus client read: %'.0lfus client write: %'.0lfus "
	     "actor: %'.0lfus queue: %'.0lfus",
	     server_stats_get_total_requests(server->server_stats),
	     server_stats_get_active_requests(server->server_stats),
	     queue_usage(server),
	     server_stats_get_time(server->server_stats, TOTAL_TIME),
	     server_stats_get_time(server->server_stats, CLIENT_READ_TIME),
	     server_stats_get_time(server->server_stats, CLIENT_WRITE_TIME),
	     server_stats_get_time(server->server_stats, ACTOR_TIME),
	     server_stats_get_time(server->server_stats, QUEUE_TIME));
  }

  return NULL;
}
