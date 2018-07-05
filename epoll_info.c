#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "epoll_info.h"
#include "logging.h"
#include "request_context.h"

EpollInfo *init_epoll_info(int accept_fd) {
  EpollInfo *epoll_info = (EpollInfo*) CHECK_MEM(calloc(1, sizeof(EpollInfo)));
  epoll_info->accept_fd = accept_fd;
  int epoll_fd = epoll_create(1);
  CHECK(epoll_fd == -1, "failed to create epoll file descriptor");
  epoll_info->epoll_fd = epoll_fd;
  return epoll_info;
}

void set_accept_epoll_event(EpollInfo *epoll) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.data.ptr = epoll;
  event.events = EPOLLIN | EPOLLET;

  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, epoll->accept_fd, &event);
  CHECK(r == -1, "failed to register accept file descriptor");  
}

void input_epoll_event(EpollInfo *epoll, RequestContext *request_context) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLET;
  event.data.ptr = request_context;
  int client_fd = rc_get_fd(request_context);
  
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
  CHECK(r == -1, "failed to add epoll event");
}

void output_epoll_event(EpollInfo *epoll, RequestContext *request_context) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLOUT | EPOLLET;
  event.data.ptr = request_context;
  int client_fd = rc_get_fd(request_context);

  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_MOD, client_fd, &event);
  CHECK(r == -1, "failed to modify epoll event");
}

void delete_epoll_event(EpollInfo *epoll, RequestContext *request_context) {
  struct epoll_event event;
  int client_fd = rc_get_fd(request_context);
  int r = epoll_ctl(epoll->epoll_fd, EPOLL_CTL_DEL, client_fd, &event);
  CHECK(r == -1, "failed to delete epoll event");
}

