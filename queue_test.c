#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <unistd.h>

#include "epoll_info.h"
#include "logging.h"
#include "queue.h"
#include "request_context.h"

#define MESSAGE_COUNT 100
#define MAX_EVENTS 10

void *send_messages(void *data) {
  LOG_INFO("Starting up the send thread");
  Queue *queue = (Queue*) data;

  for (int i = 0 ; i < MESSAGE_COUNT ; i++) {
    RequestContext *request_context = init_request_context(i, "hostname");
    enum QueueResult result = queue_push(queue, request_context);
    if (result == QUEUE_FAILURE) {
      LOG_INFO("waiting... %zu", queue_size(queue));
      sleep(1);
    }
      
  }

  return NULL;
}

void *recv_messages(void *data) {
  LOG_INFO("Starting up the recv thread");
  Queue *queue = (Queue*) data;

  EpollInfo *epoll_info = epoll_info_init("recv-messages");
  int fd = queue_add_event_fd(queue);
  add_input_epoll_event(epoll_info, fd, queue);

  int count = 0;

  while (count < MESSAGE_COUNT) {

    struct epoll_event events[MAX_EVENTS];
    sleep(5);
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "Epoll wait failed");

    for (int i = 0 ; i < ready_amount ; i++) {
      eventfd_t num_to_read;
      int r = eventfd_read(fd, &num_to_read);
      CHECK(r != 0, "Failed to read event file descriptor");
      LOG_INFO("num ready: %lu", num_to_read);

      for (uint64_t j = 0 ; j < num_to_read ; j++) {
	RequestContext *request_context;
	enum QueueResult result = queue_pop(queue, &request_context);
	CHECK(result != QUEUE_SUCCESS, "Did not successfully read message");
	LOG_INFO("received message: %d", request_context->fd);
	count++;
      }
    }
  }
  epoll_info_destroy(epoll_info);
  
  return NULL;
}

int main() {
  Queue *queue = queue_init(200);
  
  pthread_t recv_thread;
  int r = pthread_create(&recv_thread, NULL, recv_messages, queue);
  CHECK(r != 0, "Failed to create pthread");

  const char *recv_name = "recv-thrd";
  r = pthread_setname_np(recv_thread, recv_name);
  CHECK(r != 0, "Failed to set name");

  pthread_t send_thread;
  r = pthread_create(&send_thread, NULL, send_messages, queue);
  CHECK(r != 0, "Failed to create pthread");

  const char *send_name = "send-thrd";
  r = pthread_setname_np(send_thread, send_name);
  CHECK(r != 0, "Failed to set name");

  void *ptr;
  r = pthread_join(recv_thread, (void**) &ptr);
  CHECK(r != 0, "Failed to join on thread");

  queue_destroy(queue);
  LOG_INFO("Exiting mail test");
}
