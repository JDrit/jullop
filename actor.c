#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <semaphore.h>
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

  /* Now the request is blocked till the actor responds back with the
   * final result. */
  request_context->state = ACTOR_READ;

  Message message;
  message_init(&message, request_context);

  message_write_sync(fd, &message);
}

void *run_actor(void *pthread_input) {
  ActorInfo *actor_info = (ActorInfo*) pthread_input;

  // make sure all application threads have started
  pthread_barrier_wait(actor_info->startup);
  
  
  LOG_INFO("Starting actor #%d, input=%d, output=%d",
	   actor_info->id,
	   actor_info->actor_requests_fd,
	   actor_info->actor_responses_fd);

  while (1) {
    RequestContext *request_context = read_request_context(actor_info->actor_requests_fd);
    request_context->actor_id = actor_info->id;
    
    HttpRequest http_request = request_context->http_request;

    HttpHeader headers[10];
    size_t header_count = 2;


    headers[0].name = "Content-Length";
    headers[0].name_len = 14;

    char content_length_value[10];
    int size = sprintf(content_length_value, "%zu", http_request.path_len);
    CHECK(size <= 0, "Failed to print content length string");
    
    headers[0].value = content_length_value;
    headers[0].value_len = (size_t) size;

    headers[1].name = "Connection";
    headers[1].name_len = 10;
    if (context_keep_alive(request_context) == 1) {
      headers[1].value = "keep-alive";
      headers[1].value_len = 10;
    } else {
      headers[1].value = "close";
      headers[1].value_len = 5;
    }

    HttpResponse http_response;
    http_response_init(&http_response, 200, headers, header_count, http_request.path,
		       http_request.path_len);

    request_context->output_buffer = http_response.output;
    request_context->output_len = http_response.output_len;

    // forwards the response to the output actor
    write_request_context(actor_info->actor_responses_fd, request_context);
  }
  
  return NULL;
}


