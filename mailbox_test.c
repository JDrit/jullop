#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <unistd.h>

#include "epoll_info.h"
#include "logging.h"
#include "mailbox.h"

#define MESSAGE_COUNT 100
#define MAX_EVENTS 10

typedef struct Message {
  int id;
} Message;

void *send_messages(void *data) {
  LOG_INFO("Starting up the send thread");
  Mailbox *mailbox = (Mailbox*) data;

  for (int i = 0 ; i < MESSAGE_COUNT ; i++) {
    Message *message = (Message*) CHECK_MEM(malloc(sizeof(Message)));
    message->id = i;

    mailbox_send(mailbox, message);

  }

  return NULL;
}

void *recv_messages(void *data) {
  LOG_INFO("Starting up the recv thread");
  Mailbox *mailbox = (Mailbox*) data;

  EpollInfo *epoll_info = epoll_info_init("recv-messages");
  int fd = mailbox_add_event_fd(mailbox);
  add_input_epoll_event(epoll_info, fd, mailbox);

  int count = 0;

  while (count < MESSAGE_COUNT) {
    struct epoll_event events[MAX_EVENTS];
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "Epoll wait failed");

    for (int i = 0 ; i < ready_amount ; i++) {
      eventfd_t num_to_read;
      int r = eventfd_read(fd, &num_to_read);
      CHECK(r != 0, "Failed to read event file descriptor");

      for (uint64_t j = 0 ; j < num_to_read ; j++) {
	Message *message;
	enum MailResult result = mailbox_recv(mailbox, (void**) &message);
	CHECK(result != MAIL_SUCCESS, "Did not successfully read message");
	LOG_INFO("received message: %d", message->id);
	count++;
	free(message);
      }
    }
  }
  epoll_info_destroy(epoll_info);
  
  return NULL;
}

int main() {
  Mailbox *mailbox = mailbox_init(MESSAGE_COUNT);
  
  pthread_t recv_thread;
  int r = pthread_create(&recv_thread, NULL, recv_messages, mailbox);
  CHECK(r != 0, "Failed to create pthread");

  const char *recv_name = "recv-thrd";
  r = pthread_setname_np(recv_thread, recv_name);
  CHECK(r != 0, "Failed to set name");

  pthread_t send_thread;
  r = pthread_create(&send_thread, NULL, send_messages, mailbox);
  CHECK(r != 0, "Failed to create pthread");

  const char *send_name = "send-thrd";
  r = pthread_setname_np(send_thread, send_name);
  CHECK(r != 0, "Failed to set name");

  void *ptr;
  r = pthread_join(recv_thread, (void**) &ptr);
  CHECK(r != 0, "Failed to join on thread");

  mailbox_destroy(mailbox);
  LOG_INFO("Exiting mail test");
}
