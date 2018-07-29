#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "epoll_info.h"
#include "logging.h"

EpollInfo *epoll_info_init(const char *name, int id) {
  EpollInfo *epoll_info = (EpollInfo*) CHECK_MEM(calloc(1, sizeof(EpollInfo)));
  int epoll_fd = epoll_create(1);
  CHECK(epoll_fd == -1, "Failed to create epoll file descriptor");
  epoll_info->epoll_fd = epoll_fd;
  epoll_info->name = name;
  epoll_info->id = id;
  return epoll_info;
}

void epoll_info_destroy(EpollInfo *epoll_info) {  
  close(epoll_info->epoll_fd);
  free(epoll_info);
}

void add_input_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLHUP | EPOLLPRI;
  event.data.ptr = ptr;

  LOG_DEBUG("Add input event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "Failed to add input epoll event for %s", epoll->name);
}

void add_output_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT | EPOLLERR | EPOLLRDHUP | EPOLLHUP | EPOLLPRI;
  event.data.ptr = ptr;

  LOG_DEBUG("Add output event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, fd, &event);
  CHECK(r == -1, "Failed to add output epoll event for %s", epoll->name);
}

void mod_input_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLHUP | EPOLLPRI;
  event.data.ptr = ptr;

  LOG_DEBUG("Modify input event on %s for fd=%d", epoll->name, fd);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_MOD, fd, &event);
  CHECK(r == -1, "Failed to modify input epoll event for %s", epoll->name);
}

void mod_output_epoll_event(EpollInfo *epoll, int fd, void *ptr) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT | EPOLLERR | EPOLLRDHUP | EPOLLHUP | EPOLLPRI;
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

