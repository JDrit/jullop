#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include "logging.h"
#include "request_context.h"
#include "event_loop.h"

#define MAX_EVENTS 10

enum ContextType {
  CLIENT,
  ACTOR,
};

typedef struct ActorContext {
  int id;
} ActorContext;

typedef struct LoopContext {
  enum ContextType type;
  
} LoopContext;

void print_stats(EventLoop *event_loop) {
  LOG_DEBUG("selector stats");
  LOG_DEBUG("\tfd=%d", event_loop->accept_fd);
  LOG_DEBUG("\tactive_connections=%zu", event_loop->stats.active_connections);
  LOG_DEBUG("\ttotal_requests=%zu", event_loop->stats.total_requests_processed);
  LOG_DEBUG("\tbytes_read=%zu", event_loop->stats.bytes_read);
  LOG_DEBUG("\tbytes_written=%zu", event_loop->stats.bytes_written);
}

void close_request(EventLoop *event_loop,
		   RequestContext *context,
		   enum RequestResult result) {
  event_loop->stats.active_connections--;
  event_loop->stats.bytes_read += rc_get_bytes_read(context);
  event_loop->stats.bytes_written += rc_get_bytes_written(context);
  rc_finish_destroy(context, result);
}

/**
 * Checks for any error condition after waiting on the epoll event.
 * Returns 0 for success, -1 in case of error.
 */
int handle_poll_errors(EventLoop *event_loop, struct epoll_event *event) {
  RequestContext *context = (RequestContext*) event->data.ptr;
  if (event->events & EPOLLERR) {
    LOG_ERROR("EPOLLERR from event");
    close_request(event_loop, context, REQUEST_EPOLL_ERROR);
    return -1;
  } else if (event->events & EPOLLHUP) {
    LOG_ERROR("EPOLLHUP indicating peer closed the channel");
    close_request(event_loop, context, REQUEST_EPOLL_ERROR);
    return -1;
  } else {
    return 0;
  }
}

/**
 * Opens up the waiting connection from the given socket. Returns
 * the request context that will be used to handle this request.
 */
RequestContext* process_accept(EventLoop *event_loop) {
  struct sockaddr_in in_addr;
  socklen_t size = sizeof(in_addr);
  int conn_sock = accept(event_loop->accept_fd, (struct sockaddr *) &in_addr, &size);
  CHECK(conn_sock == -1, "failed to accept connection");
  fcntl(conn_sock, F_SETFL, O_NONBLOCK);

  char* hbuf = (char*) CHECK_MEM(calloc(NI_MAXHOST, sizeof(char)));
  char sbuf[NI_MAXSERV];

  int r = getnameinfo((struct sockaddr*) &in_addr, size, hbuf,
		      NI_MAXHOST * sizeof(char), sbuf,
		      sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
  CHECK(r == -1, "Failed to get host name");
  
  LOG_DEBUG("new request from %s:%s", hbuf, sbuf);
  event_loop->stats.active_connections++;
  event_loop->stats.total_requests_processed++;
  return init_request_context(conn_sock, hbuf);
}

EventLoop* init_event_loop(int accept_fd) {
  EventLoop *event = (EventLoop*) CHECK_MEM(calloc(1, sizeof(EventLoop)));
  event->accept_fd = accept_fd;
  return event;
}

/**
 * Takes a new a connection request, creates a request context for it, 
 *  and adds it onto the event loop.
 */
void handle_new_connection(EventLoop *event_loop) {
  RequestContext *context = process_accept(event_loop);
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = context;
  int conn_fd = rc_get_fd(context);
  int r = epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_ADD, conn_fd, &event);
  CHECK(r == -1, "epoll add new conneciton");
}

/**
 * Handles when the client socket is ready to be written to. Takes the output
 * buffer associated with the response and tries to flush as much as possible
 * to the socket. Cleans up the request once it has finished or an error has
 * occured.
 */
void write_client_output(EventLoop *event_loop, RequestContext *context) {
  enum WriteState state = rc_write_output(context);

  switch (state) {
  case WRITE_BUSY:
    LOG_DEBUG("socket busy");
    break;
  case WRITE_ERROR:
    LOG_ERROR("error writing output");
    close_request(event_loop, context, REQUEST_WRITE_ERROR);
    break;
  case WRITE_FINISH:
    LOG_INFO("successfully finished writing response");
    close_request(event_loop, context, REQUEST_SUCCESS);
  }
}
/**
 * Handles when the client socket is ready to be read from. Reads as much as
 * possible until a full HTTP request has been parsed.
 */
void read_client_input(EventLoop *event_loop, RequestContext *context) {
  enum ReadState state = rc_fill_input_buffer(context);

  switch (state) {
  case READ_FINISH: {
    // template output message
    char *buf = (char*) CHECK_MEM(calloc(256, sizeof(char)));
    sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nHello World", 11);
    rc_set_output_buffer(context, buf, strlen(buf));
    
    HttpRequest *request = rc_get_http_request(context);
    http_request_print(request);
    
    int fd = rc_get_fd(context);

    // Override the events to only get output events for the given connection.
    // A full HTTP request has been parsed so there is no need to read anything
    // more.
    struct epoll_event event;
    event.data.ptr = context;
    event.events = EPOLLOUT;
    
    int r = epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_MOD, fd, &event);
    CHECK(r == -1, "epoll control");
    
    //send(server->actors[0].selector_fd, buf, 256, 0);
    
    return;
  }
  case READ_ERROR:
    close_request(event_loop, context, REQUEST_READ_ERROR);
    return;
  case CLIENT_DISCONNECT:
    close_request(event_loop, context, REQUEST_CLIENT_ERROR);
    return;
  case READ_BUSY:
    // there is still more to read off of the client request, for now go
    // back onto the event loop
    return;
  }
}

/**
 * Runs the event loop and listens for connections on the given socket.
 */
void event_loop(Server *server) {
  EventLoop *event_loop = server->event_loop;
  event_loop->epoll_fd = epoll_create(1);
  CHECK(event_loop->epoll_fd == -1, "failed to create epoll loop");

  struct epoll_event event;
  event.data.ptr = event_loop;
  event.events = EPOLLIN;

  int r = epoll_ctl(event_loop->epoll_fd, EPOLL_CTL_ADD, event_loop->accept_fd, &event);
  CHECK(r == -1, "epoll control when registering accept socket");

  for (int i = 0 ; i < server->actor_count ; i++) {
  }
  
  struct epoll_event events[MAX_EVENTS];

  while (1) {
    //print_stats(event_loop);
    
    int ready_amount = epoll_wait(event_loop->epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "failed waiting on epoll");
    
    for (int i = 0 ; i < ready_amount ; i++) {
      if (handle_poll_errors(event_loop, &events[i]) != 0) {
	continue;
      }
      if (events[i].data.ptr == event_loop) {
	handle_new_connection(event_loop);
	continue;
      }

      if (events[i].events & EPOLLIN) {
	RequestContext *context = (RequestContext*) events[i].data.ptr;
	read_client_input(event_loop, context);
      }

      if (events[i].events & EPOLLOUT) {
	RequestContext *context = (RequestContext*) events[i].data.ptr;
	write_client_output(event_loop, context);
      }
    }
  }  
}

