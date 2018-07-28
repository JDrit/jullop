#ifndef __stats_h__
#define __stats_h__

#include <stdatomic.h>

typedef struct Stats {
    /* The number of currently open connections */
  atomic_long active_connections;

  /* The total number of requests that have been handled cumulatively */
  atomic_long total_requests_processed;

} Stats;

Stats *stats_init();

void stats_incr_active(Stats *stats);

void stats_decr_active(Stats *stats);

long stats_get_active(Stats *stats);

void stats_incr_total(Stats *stats);

void stats_decr_total(Stats *stats);

long stats_get_total(Stats *stats);

#endif
