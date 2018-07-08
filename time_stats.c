#include "time_stats.h"

inline static void get_time(struct timespec *time_spec) {
  clock_gettime(CLOCK_MONOTONIC, time_spec);
}

inline static time_t get_time_delta(struct timespec *start_time,
				    struct timespec *end_time) {
  struct timespec diff_time;
  diff_time.tv_sec = end_time->tv_sec - start_time->tv_sec;
  diff_time.tv_nsec = end_time->tv_nsec - start_time->tv_nsec;
  time_t microseconds = 1000000 * diff_time.tv_sec + diff_time.tv_nsec / 1000;
  return microseconds;  
}

void request_set_start(TimeStats *time_stats) {
  get_time(&time_stats->request_start);
}

void request_set_end(TimeStats *time_stats) {
  struct timespec request_end;
  get_time(&request_end);

  time_stats->request_micros += get_time_delta(&time_stats->request_start,
					       &request_end);
					       
}

time_t request_get_total_time(TimeStats *time_stats) {
  return time_stats->request_micros;
}

void request_set_client_read_start(TimeStats *time_stats) {
  get_time(&time_stats->client_start_read);
}

void request_set_client_read_end(TimeStats *time_stats) {
  struct timespec client_end_read;
  get_time(&client_end_read);

  time_stats->client_read_micros += get_time_delta(&time_stats->client_start_read,
						   &client_end_read);
}

time_t request_get_client_read_time(TimeStats *time_stats) {
  return time_stats->client_read_micros;
}

void request_set_client_write_start(TimeStats *time_stats) {
  get_time(&time_stats->client_start_write);
}

void request_set_client_write_end(TimeStats *time_stats) {
  struct timespec client_end_write;
  get_time(&client_end_write);

  time_stats->client_write_micros += get_time_delta(&time_stats->client_start_write,
						    &client_end_write);
}

time_t request_get_client_write_time(TimeStats *time_stats) {
  return time_stats->client_write_micros;
}

void request_set_actor_start(TimeStats *time_stats) {
  get_time(&time_stats->actor_start);
}

void request_set_actor_end(TimeStats *time_stats) {
  struct timespec actor_end;
  get_time(&actor_end);

  time_stats->actor_micros += get_time_delta(&time_stats->actor_start,
					     &actor_end);
}

time_t request_get_actor_time(TimeStats *time_stats) {
  return time_stats->actor_micros;
}
