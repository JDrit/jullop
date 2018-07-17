#ifndef __time_stats_h__
#define __time_stats_h__

#include <time.h>

typedef struct TimeStats {
  /* the initial start time of the request */
  struct timespec request_start;

  /* timestamp that we started reading from the client */
  struct timespec client_start_read;

  /* timestamp that we start writing to the client */
  struct timespec client_start_write;

  /* timestamp that we started writing the request to the actor */
  struct timespec actor_start;

  /* timestamp that we started writing the request to the actor */
  struct timespec mailbox_start;

  time_t request_micros;
  time_t client_read_micros;
  time_t client_write_micros;
  time_t actor_micros;
  time_t mailbox_micros;
} TimeStats;

/**
 * Loads the start time of the request.
 */
void request_set_start(TimeStats *time_stats);

/**
 * Loads the end time of the request.
 */
void request_set_end(TimeStats *time_stats);

/**
 * Calculates the total amount of time the request took in
 * microseconds.
 */
time_t request_get_total_time(TimeStats *time_stats);

/**
 * Starts calculating the amount of time spent reading from the client
 */
void request_set_client_read_start(TimeStats *time_stats);

/**
 * Uses the previously set client start read time to calculate the amount of
 * time spent reading from the client.
 */
void request_set_client_read_end(TimeStats *time_stats);

/**
 * Returns the total amount of time that was spent reading
 * from the client.
 */
time_t request_get_client_read_time(TimeStats *time_stats);

/**
 * Starts calculating the amount of time spent writing to the client. Uses
 * the current time.
 */
void request_set_client_write_start(TimeStats *time_stats);

/**
 * Uses the previously set client start write time to calculate the amount
 * of time spent writing to the client.
 */
void request_set_client_write_end(TimeStats *time_stats);

/**
 * Returns the total amount of time that was spent writing to the client.
 */
time_t request_get_client_write_time(TimeStats *time_stats);

/**
 * Starts calculating the amount of time that the actor was processing
 * the request.
 */
void request_set_actor_start(TimeStats *time_stats);

/**
 * Uses the previously set actor start time to calculate the amount of
 * time spent processing the request.
 */
void request_set_actor_end(TimeStats *time_stats);

/**
 * Returns the total amount of time that the actor spent processing the
 * request.
 */
time_t request_get_actor_time(TimeStats *time_stats);

/**
 * Stats calculating the amount of time that the request spends in the
 * queue.
 */
void request_set_queue_start(TimeStats *time_stats);

/**
 * Uses the previously set queue start time to calculate the amount of
 * time spent in the queue.
 */
void request_set_queue_end(TimeStats *time_stats);

/**
 * Returns the amount of time that the request spent in the queue.
 */
time_t request_get_queue_time(TimeStats *time_stats);

#endif
