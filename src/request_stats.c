#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "logging.h"
#include "request_stats.h"

static inline const char *time_name(enum TimeType type) {
  switch (type) {
  case TOTAL_TIME:
    return "TOTAL_TIME";
  case CLIENT_READ_TIME:
    return "CLIENT_READ_TIME";
  case CLIENT_WRITE_TIME:
    return "CLIENT_WRITE_TIME";
  case ACTOR_TIME:
    return "ACTOR_TIME";
  case QUEUE_TIME:
    return "QUEUE_TIME";
  default:
    return "UNKNOWN_TIME";
  }
}

static inline void get_time(struct timespec *time_spec) {
  clock_gettime(CLOCK_MONOTONIC, time_spec);
}

static inline time_t get_time_delta(struct timespec *start_time,
				    struct timespec *end_time) {
  struct timespec diff_time;
  diff_time.tv_sec = end_time->tv_sec - start_time->tv_sec;
  diff_time.tv_nsec = end_time->tv_nsec - start_time->tv_nsec;
  time_t microseconds = 1000000 * diff_time.tv_sec + diff_time.tv_nsec / 1000;
  return microseconds;  
}

void per_request_clear_time(PerRequestStats *stats) {
  stats->is_time_set = 0;
  memset(stats->start, 0, sizeof(struct timespec) * 5);
  memset(stats->total_micros, 0, sizeof(time_t) * 5);
}
bool per_request_is_time_set(PerRequestStats *stats, enum TimeType type) {
  if ((stats->is_time_set & (1 << type)) != 0) {
    return true;
  } else {
    return false;
  }
}

void per_request_record_start(PerRequestStats *stats, enum TimeType type) {
  CHECK((stats->is_time_set & (1 << type)) != 0,
	"start time already set for %s", time_name(type));
  stats->is_time_set |= (1 << type);

  get_time(&stats->start[type]);
}

void per_request_record_end(PerRequestStats *stats, enum TimeType type) {
  CHECK((stats->is_time_set & (1 << type)) == 0,
	"start time not set for %s", time_name(type));

  // resets the check
  stats->is_time_set &= ~(1 << type);
  
  struct timespec start = stats->start[type];
  struct timespec end;
  get_time(&end);

  stats->total_micros[type] += get_time_delta(&start, &end);
}

time_t per_request_get_time(PerRequestStats *stats, enum TimeType type) {
  return stats->total_micros[type];
}
