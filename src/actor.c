#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <semaphore.h>
#include <unistd.h>

#include "actor.h"
#include "epoll_info.h"
#include "http_request.h"
#include "http_response.h"
#include "logging.h"
#include "queue.h"
#include "request_context.h"
#include "server.h"
#include "time_stats.h"

#define MAX_EVENTS 10

/**
 * Does the actual request processing. Takes in a request context and is
 * responsible for constructing the HTTP response and storing it in the
 * output buffer.
 */
static void handle_request(ActorInfo *actor_info, RequestContext *request_context) {
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

    http_response_init(request_context->output_buffer, 200, headers,
		       header_count, http_request.path,
		       http_request.path_len);
}

void process_epoll_event(ActorInfo *actor_info) {
  while (1) {
    eventfd_t num_to_read;
    int r = eventfd_read(queue_add_event_fd(actor_info->input_queue), &num_to_read);
    
    if (r == -1 || num_to_read == 0) {
      return;
    }
    
    for (eventfd_t i = 0 ;  i < num_to_read ; i++) {
      RequestContext *request_context  = queue_pop(actor_info->input_queue);
      if (request_context == NULL) {
	return;
      }
      
      /* process the actor request and generate a response. */
      request_record_start(&request_context->time_stats, ACTOR_TIME);
      
      handle_request(actor_info, request_context);
      
      request_record_end(&request_context->time_stats, ACTOR_TIME);
      
      enum QueueResult result = queue_push(actor_info->output_queue, request_context);
      CHECK(result != QUEUE_SUCCESS, "Failed to send request context back");
    }
  }
}

void *run_actor(void *pthread_input) {
  ActorInfo *actor_info = (ActorInfo*) pthread_input;

  // make sure all application threads have started
  pthread_barrier_wait(actor_info->startup);
  
  LOG_INFO("Starting actor #%d", actor_info->id);

  const char *name = "actor-epoll";
  EpollInfo *epoll_info = epoll_info_init(name);

  Queue *input_queue = actor_info->input_queue;
  int event_fd = queue_add_event_fd(input_queue);
  
  add_input_epoll_event(epoll_info, event_fd, input_queue);

  struct epoll_event events[MAX_EVENTS];
  while (1) {
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "Failed to wait on epoll");

    process_epoll_event(actor_info);
  }
  
  return NULL;
}
