#ifndef __dbg_h__
#define __dbg_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef DEBUG

#define LOG_DEBUG(M, ...) \
  fprintf(stderr, "[DEBUG]\t(%s:%d):\t" M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(M, ...)
#endif

#define CLEAN_ERRNO() (errno == 0 ? "None" : strerror(errno))

#define LOG_ERROR(M, ...) \
  fprintf(stderr, "[ERROR]\t(%s:%d: errno: %s) " M "\n", __FILE__,	\
	  __LINE__, CLEAN_ERRNO(), ##__VA_ARGS__)

#define LOG_WARN(M, ...) \
  fprintf(stderr, "[WARN]\t(%s:%d: errno: %d %s) " M "\n", __FILE__,	\
	  __LINE__, errno, CLEAN_ERRNO(), ##__VA_ARGS__)

#define LOG_INFO(M, ...) \
  fprintf(stderr, "[INFO]\t(%s:%d):\t" M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define CHECK(A, M, ...) \
  if((A)) {				    \
    LOG_ERROR(M, ##__VA_ARGS__);	    \
    exit(EXIT_FAILURE);			    \
  }

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

#define CHECK_DEBUG(A, M, ...) \
  if(!(A)) {			     \
    LOG_DEBUG(M, ##__VA_ARGS__);     \
    exit(EXIT_FAILURE);		     \
  }

#endif
