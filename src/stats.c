#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdlib.h>

#include "logging.h"
#include "stats.h"

Stats *stats_init() {
  Stats *stats = (Stats*) CHECK_MEM(malloc(sizeof(Stats)));
  stats->active_connections = ATOMIC_VAR_INIT(0);
  stats->total_requests_processed = ATOMIC_VAR_INIT(0);
  return stats;
}

void stats_destroy(Stats *stats) {
  free(stats);
}

inline void stats_incr_active(Stats *stats) {
  atomic_fetch_add_explicit(&stats->active_connections, 1, memory_order_relaxed);
}

inline void stats_decr_active(Stats *stats) {
  atomic_fetch_add_explicit(&stats->active_connections, -1, memory_order_relaxed);
}

inline long stats_get_active(Stats *stats) {
  return atomic_load_explicit(&stats->active_connections, memory_order_relaxed);
}

inline void stats_incr_total(Stats *stats) {
  atomic_fetch_add_explicit(&stats->total_requests_processed, 1, memory_order_relaxed);
}

inline void stats_decr_total(Stats *stats) {
  atomic_fetch_add_explicit(&stats->total_requests_processed, -1, memory_order_relaxed);
}

inline long stats_get_total(Stats *stats) {
  return atomic_load_explicit(&stats->total_requests_processed, memory_order_relaxed);
}
