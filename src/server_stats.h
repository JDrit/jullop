#ifndef __stats_h__
#define __stats_h__

#include <stdatomic.h>
#include <time.h>

#include "request_stats.h"

typedef struct ServerWideStats {
  /* keeps track of the time spent on each type for the whole 
   * server. */
  atomic_long avg_micros[5];

  /* The number of currently open connections */
  atomic_long active_connections;

  /* The total number of requests that have been handled cumulatively */
  atomic_long total_requests_processed;
  
} ServerWideStats;


ServerWideStats *server_stats_init();

void server_stats_destroy(ServerWideStats *stats);

/**
 * Add the stats for the given request to the server-wide statistics
 * tracker. This should be done at the end of a request flow. 
 */
void server_stats_record_request(ServerWideStats *server_stats,
				 PerRequestStats *request_stats);


/**
 * Fetches the average time the server spent on the given type of time
 * tracker. This is averaged over a the most recent requests.
 */
double server_stats_get_time(ServerWideStats *server_stats,
			  enum TimeType type);

/**
 * Increments the count of the current active request count.
 */
void server_stats_incr_active_requests(ServerWideStats *server_stats);

/**
 * Decrements the count of hte current active request count.
 */
void server_stats_decr_active_requests(ServerWideStats *server_stats);

/**
 * Returns the amount of requests that are currently being
 * processed.
 */
long server_stats_get_active_requests(ServerWideStats *server_stats);

/**
 * Increments the count of the total number of requests
 * processed.
 */
void server_stats_incr_total_requests(ServerWideStats *server_stats);

/**
 * Decrements the count of the total number of requests
 * processed.
 */
void server_stats_decr_total_requests(ServerWideStats *server_stats);

/**
 * Returns the amount of requests that the server has
 * processed.
 */
long server_stats_get_total_requests(ServerWideStats *server_stats);

#endif
