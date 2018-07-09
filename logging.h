#ifndef __logging_h__
#define __logging_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

// Check if the given error condition is due to
// blocking IO
#define ERROR_BLOCK errno == EAGAIN || errno == EWOULDBLOCK

#define CLEAN_ERRNO() (errno == 0 ? "None" : strerror(errno))

#define LOG(level, M, ...) {						\
    char __thread_buffer[20];						\
  pthread_t __thread_id = pthread_self();				\
    pthread_getname_np(__thread_id, __thread_buffer, 20);			\
  fprintf(stderr, "[" level "] [%s] (%s:%d) " M "\n",			\
	  __thread_buffer, __FILE__, __LINE__, ##__VA_ARGS__);		\
  }

#define ERR_LOG(level, M, ...) {					\
  char __thread_buffer[20];					\
  pthread_t __thread_id = pthread_self();				\
  pthread_getname_np(__thread_id, __thread_buffer, 20);			\
  if (errno == 0) {							\
    fprintf(stderr, "[" level "] [%s] (%s:%d) " M "\n",			\
	    __thread_buffer, __FILE__, __LINE__, ##__VA_ARGS__);	\
  } else {								\
    fprintf(stderr, "[" level "] [%s] (%s:%d) (errno: %d %s) " M "\n",	\
	    __thread_buffer, __FILE__, __LINE__, errno, CLEAN_ERRNO(), \
	    ##__VA_ARGS__);						\
  }									\
  }

#define LOG_INFO(M, ...) LOG("INFO ", M, ##__VA_ARGS__); 

#define LOG_WARN(M, ...) ERR_LOG("WARN ", M, ##__VA_ARGS__);

#define LOG_ERROR(M, ...) ERR_LOG("ERROR", M, ##__VA_ARGS__);

#define CHECK(A, M, ...) \
  if((A)) {				    \
    LOG_ERROR(M, ##__VA_ARGS__);	    \
    exit(EXIT_FAILURE);			    \
  }

#define FAIL(M, ...) CHECK(1, M, ##__VA_ARGS__);

#define sentinel(M, ...) { \
  LOG_ERROR(M, ##__VA_ARGS__);		\
  exit(EXIT_FAILURE);			\
}

#define CHECK_MEM(x) ({				\
      void* ret_val = x;				\
      if (ret_val == NULL) {			\
	LOG_ERROR("Out of Memory");		\
	exit(EXIT_FAILURE);			\
      }						\
      ret_val;					\
})

#ifdef DEBUG

#define LOG_DEBUG(M, ...) LOG("DEBUG", M, ##__VA_ARGS__);

#define CHECK_DEBUG(A, M, ...) \
  if(!(A)) {			     \
    LOG_DEBUG(M, ##__VA_ARGS__);     \
    exit(EXIT_FAILURE);		     \
  }

#else

#define LOG_DEBUG(M, ...)

#define CHECK_DEBUG(A, M, ...)

#endif


#endif
