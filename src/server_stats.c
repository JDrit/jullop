#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdlib.h>

#include "logging.h"
#include "request_stats.h"
#include "server_stats.h"

#define N 100.0

#define ATOMIC_ADD(ATOMIC, VALUE) \
  atomic_fetch_add_explicit((ATOMIC), (VALUE), memory_order_relaxed);

ServerWideStats *server_stats_init() {
  ServerWideStats *stats = (ServerWideStats*) CHECK_MEM(malloc(sizeof(ServerWideStats)));

  for (int i = 0 ; i < 5 ; i++) {
    stats->avg_micros[i] = ATOMIC_VAR_INIT(0);
  }
  
  stats->active_connections = ATOMIC_VAR_INIT(0);
  stats->total_requests_processed = ATOMIC_VAR_INIT(0);
  return stats;
}

void server_stats_destroy(ServerWideStats *stats) {
  free(stats);
}

void server_stats_record_request(ServerWideStats *server_stats,
				 PerRequestStats *request_stats) {

  time_t total_time = per_request_get_time(request_stats, TOTAL_TIME);
  ATOMIC_ADD(&server_stats->avg_micros[TOTAL_TIME], total_time);

  time_t client_read = per_request_get_time(request_stats, CLIENT_READ_TIME);
  ATOMIC_ADD(&server_stats->avg_micros[CLIENT_READ_TIME], client_read);

  time_t client_write = per_request_get_time(request_stats, CLIENT_WRITE_TIME);
  ATOMIC_ADD(&server_stats->avg_micros[CLIENT_WRITE_TIME], client_write);
  
  time_t actor_time = per_request_get_time(request_stats, ACTOR_TIME);
  ATOMIC_ADD(&server_stats->avg_micros[ACTOR_TIME], actor_time);

  time_t queue_time = per_request_get_time(request_stats, QUEUE_TIME);
  ATOMIC_ADD(&server_stats->avg_micros[QUEUE_TIME], queue_time);
}

double server_stats_get_time(ServerWideStats *server_stats, enum TimeType type) {
  /* the time here is given as an estimate so we can afford to use a weaker
   * memory model to help the compiler out. */
  long time = atomic_load_explicit(&server_stats->avg_micros[type], memory_order_relaxed);
  long requests = atomic_load_explicit(&server_stats->total_requests_processed, memory_order_relaxed);
  return (1.0 * time) / requests;
}

inline void server_stats_incr_active_requests(ServerWideStats *stats) {
  atomic_fetch_add_explicit(&stats->active_connections, 1, memory_order_relaxed);
}

inline void server_stats_decr_active_requests(ServerWideStats *stats) {
  atomic_fetch_add_explicit(&stats->active_connections, -1, memory_order_relaxed);
}

inline long server_stats_get_active_requests(ServerWideStats *stats) {
  return atomic_load_explicit(&stats->active_connections, memory_order_relaxed);
}

inline void server_stats_incr_total_requests(ServerWideStats *stats) {
  atomic_fetch_add_explicit(&stats->total_requests_processed, 1, memory_order_relaxed);
}

inline void server_stats_decr_total_requests(ServerWideStats *stats) {
  atomic_fetch_add_explicit(&stats->total_requests_processed, -1, memory_order_relaxed);
}

inline long server_stats_get_total_requests(ServerWideStats *stats) {
  return atomic_load_explicit(&stats->total_requests_processed, memory_order_relaxed);
}
