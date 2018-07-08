#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "epoll_info.h"
#include "logging.h"

EpollInfo *init_epoll_info() {
  EpollInfo *epoll_info = (EpollInfo*) CHECK_MEM(calloc(1, sizeof(EpollInfo)));
  int epoll_fd = epoll_create(1);
  CHECK(epoll_fd == -1, "failed to create epoll file descriptor");
  epoll_info->epoll_fd = epoll_fd;
  return epoll_info;
}

void set_accept_epoll_event(EpollInfo *epoll, int fd) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.data.ptr = epoll;
  event.events = EPOLLIN | EPOLLET;

  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "failed to register accept file descriptor");  
}

enum EpollError check_epoll_errors(EpollInfo *epoll, struct epoll_event *event) {
  if (event->events & EPOLLERR) {
    LOG_WARN("Epoll event error due to read side of the socket closing");
    return EPOLL_ERROR;
  } else if (event->events & EPOLLHUP) {
    LOG_WARN("Epoll event error due to peer closing connection");    
    return EPOLL_ERROR;
  } else {
    return NONE;
  }
}

void add_input_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLET;
  event.data.ptr = ptr;

  LOG_DEBUG("add epoll=%d fd=%d to accept reads",
	    epoll->epoll_fd, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "failed to add input epoll event");
}

void add_output_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT | EPOLLET;
  event.data.ptr = ptr;

  LOG_DEBUG("add epoll=%d fd=%d to accept writes",
	    epoll->epoll_fd, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "failed to add output epoll event");
}

void mod_input_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLET;
  event.data.ptr = ptr;

  LOG_DEBUG("modify epoll=%d fd=%d to accept reads",
	    epoll->epoll_fd, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_MOD, fd, &event);
  CHECK(r == -1, "failed to modify input epoll event");
}

void mod_output_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT | EPOLLET;
  event.data.ptr = ptr;

  LOG_DEBUG("modify epoll=%d fd=%d to accept write",
	    epoll->epoll_fd, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_MOD, fd, &event);
  CHECK(r == -1, "failed to modify output epoll event");
}

void delete_epoll_event(EpollInfo *epoll, int fd) {
  struct epoll_event event;

  LOG_DEBUG("delete epoll=%d fd=%d to accept all operations",
	    epoll->epoll_fd, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_DEL, fd, &event);
  CHECK(r == -1, "failed to delete epoll event for epoll=%d fd=%d",
	epoll->epoll_fd, fd);
}

