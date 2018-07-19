#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>

#include "logging.h"
#include "time_stats.h"

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
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, time_spec);
}

static inline time_t get_time_delta(struct timespec *start_time,
				    struct timespec *end_time) {
  struct timespec diff_time;
  diff_time.tv_sec = end_time->tv_sec - start_time->tv_sec;
  diff_time.tv_nsec = end_time->tv_nsec - start_time->tv_nsec;
  time_t microseconds = 1000000 * diff_time.tv_sec + diff_time.tv_nsec / 1000;
  return microseconds;  
}

void request_clear_time(TimeStats *time_stats) {
  time_stats->is_time_set = 0;
}

void request_record_start(TimeStats *time_stats, enum TimeType type) {
  CHECK((time_stats->is_time_set & (1 << type)) != 0,
	"start time already set for %s", time_name(type));
  time_stats->is_time_set |= (1 << type);

  get_time(&time_stats->start[type]);
}

void request_record_end(TimeStats *time_stats, enum TimeType type) {
  CHECK((time_stats->is_time_set & (1 << type)) == 0,
	"start time not set for %s", time_name(type));

  // resets the check
  time_stats->is_time_set &= ~(1 << type);
  
  struct timespec start = time_stats->start[type];
  struct timespec end;
  get_time(&end);

  time_stats->total_micros[type] += get_time_delta(&start, &end);
}

time_t request_get_time(TimeStats *time_stats, enum TimeType type) {
  return time_stats->total_micros[type];
}
