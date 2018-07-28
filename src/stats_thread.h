#ifndef __stats_thread_h__
#define __stats_thread_h__

/**
 * A separate thread that is used to print request statistics
 * to stdout.
 */
void *stats_loop(void *pthread_input);

#endif
