#ifndef __logging_h__
#define __logging_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define ANSI_COLOR_RESET   "\x1b[0m"
#define DEBUG_COLOR "\x1b[36m"
#define INFO_COLOR  "\x1b[32m"
#define WARN_COLOR  "\x1b[33m"
#define ERROR_COLOR "\x1b[31m"
#define FATAL_COLOR "\x1b[1;31m"

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// Check if the given error condition is due to
// blocking IO
#define ERROR_BLOCK errno == EAGAIN || errno == EWOULDBLOCK

#define CLEAN_ERRNO() (errno == 0 ? "None" : strerror(errno))

#define LOG(level_name, level_color, M, ...) {				\
  char __thread_buffer[20];						\
  pthread_t __thread_id = pthread_self();				\
  pthread_getname_np(__thread_id, __thread_buffer, 20);			\
  fprintf(stderr, "[" level_color level_name ANSI_COLOR_RESET "] [%s] (%s:%d) " M "\n",	\
	  __thread_buffer, __FILE__, __LINE__, ##__VA_ARGS__);		\
  }

#define ERR_LOG(level_name, level_color, M, ...) {			\
  char __thread_buffer[20];						\
  pthread_t __thread_id = pthread_self();				\
  pthread_getname_np(__thread_id, __thread_buffer, 20);			\
  if (errno == 0) {							\
    fprintf(stderr, "[" level_color level_name ANSI_COLOR_RESET "] [%s] (%s:%d) " M "\n", \
	    __thread_buffer, __FILE__, __LINE__, ##__VA_ARGS__);	\
  } else {								\
    fprintf(stderr, "[" level_color level_name ANSI_COLOR_RESET "] [%s] (%s:%d) (errno: %d %s) " M "\n", \
	    __thread_buffer, __FILE__, __LINE__, errno, CLEAN_ERRNO(),	\
	    ##__VA_ARGS__);						\
  }									\
  }

#define LOG_INFO(M, ...) LOG("INFO ", INFO_COLOR, M, ##__VA_ARGS__); 

#define LOG_WARN(M, ...) ERR_LOG("WARN ", WARN_COLOR, M, ##__VA_ARGS__);

#define LOG_ERROR(M, ...) ERR_LOG("ERROR", ERROR_COLOR, M, ##__VA_ARGS__);

#define LOG_FATAL(M, ...) ERR_LOG("FATAL", FATAL_COLOR, M, ##__VA_ARGS__);

#define CHECK(A, M, ...)		    \
  if(UNLIKELY((A))) {			    \
    LOG_FATAL(M, ##__VA_ARGS__);	    \
    exit(EXIT_FAILURE);			    \
  }

#define FAIL(M, ...) CHECK(1, M, ##__VA_ARGS__);

#define CHECK_MEM(x) ({			        \
  void* ret_val = x;	                        \
  if (UNLIKELY(ret_val == NULL)) {              \
    LOG_FATAL("Out of Memory");			\
    exit(EXIT_FAILURE);				\
  }						\
  ret_val;					\
})

#ifdef DEBUG

#define LOG_DEBUG(M, ...) LOG("DEBUG", DEBUG_COLOR, M, ##__VA_ARGS__);

#else

#define LOG_DEBUG(M, ...)

#endif


#endif
