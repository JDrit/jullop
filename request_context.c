
#include <stdlib.h>
#include "logging.h"

#define BUF_SIZE 2048

struct RequestContext {
  char *remote_host;
  int fd;
  char *buffer;
};


struct RequestContext* init_request_context(int fd) {
  struct RequestContext *context = (struct RequestContext*)
    CHECK_MEM(calloc(1, sizeof(struct RequestContext)));
  context->fd = fd;
  context->buffer = (char*) CHECK_MEM(calloc(BUF_SIZE, sizeof(char)));
  return context;
}

int rc_get_fd(struct RequestContext *context) {
  return context->fd;
}

void destroy_request_context(struct RequestContext *context) {
  free(context->buffer);
  free(context);
}
