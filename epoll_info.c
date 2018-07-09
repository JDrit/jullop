#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "epoll_info.h"
#include "logging.h"

EpollInfo *init_epoll_info(const char *name) {
  EpollInfo *epoll_info = (EpollInfo*) CHECK_MEM(calloc(1, sizeof(EpollInfo)));
  int epoll_fd = epoll_create(1);
  CHECK(epoll_fd == -1, "Failed to create epoll file descriptor");
  epoll_info->epoll_fd = epoll_fd;
  epoll_info->name = name;
  return epoll_info;
}

void epoll_info_print(EpollInfo *epoll) {
  LOG_INFO("name='%s' active=%ld total=%ld bytes_read=%ld bytes_written=%ld",
	   epoll->name,
	   epoll->stats.active_connections,
	   epoll->stats.total_requests_processed,
	   epoll->stats.bytes_read,
	   epoll->stats.bytes_written);
}

void set_accept_epoll_event(EpollInfo *epoll, int fd) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.data.ptr = epoll;
  event.events = EPOLLIN | EPOLLET;

  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "Failed to register accept file descriptor for %s",
	epoll->name);  
}

enum EpollError check_epoll_errors(EpollInfo *epoll, struct epoll_event *event) {
  if (event->events & EPOLLERR) {
    LOG_WARN("Epoll event error on %s due to read side of the socket closing",
	     epoll->name);
    CHECK(1, "fail");
    return EPOLL_ERROR;
  } else if (event->events & EPOLLHUP) {
    LOG_WARN("Epoll event error on %s due to peer closing connection",
	     epoll->name);
    return EPOLL_ERROR;
  } else {
    return NONE;
  }
}

void add_input_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN;
  event.data.ptr = ptr;

  LOG_DEBUG("Add input event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "Failed to add input epoll event for %s", epoll->name);
}

void add_output_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT;
  event.data.ptr = ptr;

  LOG_DEBUG("Add output event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "Failed to add output epoll event for %s", epoll->name);
}

void mod_input_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN;
  event.data.ptr = ptr;

  LOG_DEBUG("Modify input event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_MOD, fd, &event);
  CHECK(r == -1, "Failed to modify input epoll event for %s", epoll->name);
}

void mod_output_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT;
  event.data.ptr = ptr;

  LOG_DEBUG("Modify output event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_MOD, fd, &event);
  CHECK(r == -1, "Failed to modify output event for %s", epoll->name);
}

void delete_epoll_event(EpollInfo *epoll, int fd) {
  struct epoll_event event;

  LOG_DEBUG("Delete event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_DEL, fd, &event);
  CHECK(r == -1, "Failed to delete event for %s", epoll->name);
}

