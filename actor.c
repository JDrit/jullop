#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "actor.h"
#include "http_request.h"
#include "http_response.h"
#include "logging.h"
#include "message_passing.h"
#include "request_context.h"
#include "time_stats.h"

static RequestContext *read_request_context(int fd) {
  Message message;
  message_reset(&message);
  
  message_read_sync(fd, &message);
  
  RequestContext *request_context = (RequestContext*) message_get_payload(&message);
  
  // Since the request context was created on the input actor thread, this makes sure
  // that the application-level actor is able to see it.
  __sync_synchronize();
  request_set_actor_start(&request_context->time_stats);
  return request_context;
}

static void write_request_context(int fd, RequestContext *request_context) {
  request_set_actor_end(&request_context->time_stats);
  
  Message message;
  message_init(&message, request_context);

  message_write_sync(fd, &message);
}

void *run_actor(void *pthread_input) {
    
  ActorInfo *actor_info = (ActorInfo*) pthread_input;
  LOG_DEBUG("Starting actor #%d, input=%d, output=%d",
	    actor_info->id,
	    actor_info->actor_requests_fd,
	    actor_info->actor_responses_fd);

  while (1) {
    RequestContext *request_context = read_request_context(actor_info->actor_requests_fd);
    
    
    HttpRequest http_request = request_context->http_request;
    LOG_INFO("actor %d received request: %.*s %.*s",
	     actor_info->id,
	     (int) http_request.method_len, http_request.method,
	     (int) http_request.path_len, http_request.path);

    HttpResponse http_response;
    init_http_response(&http_response, 200, http_request.path, http_request.path_len);

    request_context->output_buffer = http_response.output;
    request_context->output_len = http_response.output_len;

    LOG_DEBUG("Actor %d starting to write response", actor_info->id);
    write_request_context(actor_info->actor_responses_fd, request_context);
    LOG_DEBUG("Actor %d finished writing response", actor_info->id);
  }
  
  return NULL;
}


