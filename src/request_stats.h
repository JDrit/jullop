#ifndef __time_stats_h__
#define __time_stats_h__

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

enum TimeType {
  TOTAL_TIME = 0,
  CLIENT_READ_TIME = 1,
  CLIENT_WRITE_TIME = 2,
  ACTOR_TIME = 3,
  QUEUE_TIME = 4,
};

/* Stats that are only relevent to a single request. */
typedef struct PerRequestStats {

  /* the initial start time of the time type. */
  struct timespec start[5];

  /* the total amount of time in microseconds that the measurement used. */
  time_t total_micros[5];

  /* used to detect if the time has already been started for a particular type. */
  uint8_t is_time_set;
  
} PerRequestStats;


/**
 * Wipes out all the time values collected so far.
 */
void per_request_clear_time(PerRequestStats *stats);

/**
 * Checks to see if the request has set the given time type.
 */
bool per_request_is_time_set(PerRequestStats *stats, enum TimeType type);

/**
 * Records the start time for the given time measurement.
 */
void per_request_record_start(PerRequestStats *stats, enum TimeType type);

/**
 * Records the end time for the given time measurement.
 */
void per_request_record_end(PerRequestStats *stats, enum TimeType type);

/**
 * Returns the amount of time in microseconds for the given time measurement.
 */
time_t per_request_get_time(PerRequestStats *stats, enum TimeType type);

#endif
