#ifndef __time_stats_h__
#define __time_stats_h__

#include <time.h>
#include <stdint.h>

enum TimeType {
  TOTAL_TIME = 0,
  CLIENT_READ_TIME = 1,
  CLIENT_WRITE_TIME = 2,
  ACTOR_TIME = 3,
  QUEUE_TIME = 4,
};

typedef struct TimeStats {

  /* the initial start time of the time type. */
  struct timespec start[5];

  /* the total amount of time in microseconds that the measurement used. */
  time_t total_micros[5];

  /* used to detect if the time has already been started for a particular type. */
  uint8_t is_time_set;
  
} TimeStats;

/**
 * Wipes out all the time values collected so far.
 */
void request_clear_time(TimeStats *time_stats);

/**
 * Records the start time for the given time measurement.
 */
void request_record_start(TimeStats *time_stats, enum TimeType type);

/**
 * Records the end time for the given time measurement.
 */
void request_record_end(TimeStats *time_stats, enum TimeType type);

/**
 * Returns the amount of time in microseconds for the given time measurement.
 */
time_t request_get_time(TimeStats *time_stats, enum TimeType type);

#endif
