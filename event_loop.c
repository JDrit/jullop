#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "epoll_info.h"
#include "event_loop.h"
#include "logging.h"
#include "request_context.h"

#define MAX_EVENTS 10

/**
 * Cleans up client requests.
 */
void close_client_request(EpollInfo *epoll_info,
			  RequestContext *request_context,
			  enum RequestResult result) {
  epoll_info->stats.active_connections--;
  epoll_info->stats.bytes_read += context_bytes_read(request_context);
  epoll_info->stats.bytes_written += context_bytes_written(request_context);
  delete_epoll_event(epoll_info, request_context->fd);
  request_finish_destroy(request_context, result);
}

/**
 * Checks for any error condition after waiting on the epoll event.
 * Returns 0 for success, -1 in case of error.
 */
int handle_poll_errors(EpollInfo *epoll_info, struct epoll_event *event) {
  RequestContext *request_context = (RequestContext*) event->data.ptr;

  if (event->events & EPOLLERR || event->events & EPOLLHUP) {
    enum RequestState state = request_context->state;

    if (state == CLIENT_READ || state == CLIENT_WRITE) {
      LOG_ERROR("error from client event");
      close_client_request(epoll_info, request_context, REQUEST_EPOLL_ERROR);
    } else if (state == ACTOR_READ || state == ACTOR_WRITE) {      
      LOG_ERROR("error from actor event");
    }
    return -1;
  } else {
    return 0;
  }
}

/**
 * Opens up the waiting connection from the given socket. Returns
 * the request context that will be used to handle this request.
 */
RequestContext* process_accept(EpollInfo *epoll_info) {
  struct sockaddr_in in_addr;
  socklen_t size = sizeof(in_addr);
  int conn_sock = accept(epoll_info->accept_fd, (struct sockaddr *) &in_addr, &size);
  CHECK(conn_sock == -1, "failed to accept connection");
  fcntl(conn_sock, F_SETFL, O_NONBLOCK);

  char* hbuf = (char*) CHECK_MEM(calloc(NI_MAXHOST, sizeof(char)));
  char sbuf[NI_MAXSERV];

  int r = getnameinfo((struct sockaddr*) &in_addr, size, hbuf,
		      NI_MAXHOST * sizeof(char), sbuf,
		      sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
  CHECK(r == -1, "Failed to get host name");
  
  LOG_DEBUG("new request from %s:%s", hbuf, sbuf);
  epoll_info->stats.active_connections++;
  epoll_info->stats.total_requests_processed++;
  return init_request_context(conn_sock, hbuf);
}

/**
 * Handles when the client socket is ready to be written to. Takes the output
 * buffer associated with the response and tries to flush as much as possible
 * to the socket. Cleans up the request once it has finished or an error has
 * occured.
 */
void write_client_response(EpollInfo *epoll_info,
			   RequestContext *request_context) {
  enum WriteState state = context_write_client_response(request_context);

  switch (state) {
  case WRITE_BUSY:
    LOG_DEBUG("socket busy");
    break;
  case WRITE_ERROR:
    LOG_ERROR("error writing output");
    request_context->state = FINISH;
    close_client_request(epoll_info, request_context, REQUEST_WRITE_ERROR);
    break;
  case WRITE_FINISH:
    LOG_INFO("successfully finished writing response");
    request_context->state = FINISH;
    close_client_request(epoll_info, request_context, REQUEST_SUCCESS);
    break;
  }
}

void write_actor_request(EpollInfo *epoll_info, RequestContext *request_context) {
  enum WriteState state = context_write_actor_request(request_context);

  switch (state) {
  case WRITE_BUSY:
    LOG_DEBUG("actor socket busy");
    break;
  case WRITE_ERROR:
    LOG_ERROR("error writing to the actor socket");
    break;
  case WRITE_FINISH:
    // wait for the actor to respond.
    request_context->state = ACTOR_READ;
    ActorInfo *actor = context_get_actor(request_context);
    add_input_epoll_event(epoll_info, actor->selector_fd, request_context);
    break;
  }
}

void read_actor_response(EpollInfo *epoll_info, RequestContext *request_context) {
  enum ReadState state = context_read_actor_response(request_context);

  switch (state) {
  case READ_BUSY:
    LOG_DEBUG("actor socket busy");
    break;
  case READ_ERROR:
    LOG_ERROR("error reading from actor socket");
    break;
  case CLIENT_DISCONNECT:
    LOG_ERROR("client error reading from actor socket");
    break;
  case READ_FINISH:
    request_context->state = CLIENT_WRITE;
    add_output_epoll_event(epoll_info, request_context->fd, request_context);
  }
}

/**
 * Handles when the client socket is ready to be read from. Reads as much as
 * possible until a full HTTP request has been parsed.
 */
void read_client_request(EpollInfo *epoll_info, Server *server,
			 RequestContext *request_context) {
  
  enum ReadState state = context_read_client_request(request_context);

  switch (state) {
  case READ_FINISH: {
    ActorInfo *actor_info = &server->actors[0];

    request_context->state = ACTOR_WRITE;
    context_set_actor(request_context, actor_info);

    // registers the communication layer to the actor
    output_epoll_event(epoll_info, actor_info->selector_fd, request_context);
    return;
  }
  case READ_ERROR:
    request_context->state = FINISH;
    close_client_request(epoll_info, request_context, REQUEST_READ_ERROR);
    return;
  case CLIENT_DISCONNECT:
    request_context->state = FINISH;
    close_client_request(epoll_info, request_context, REQUEST_CLIENT_ERROR);
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
void event_loop(Server *server, int sock_fd) {
  EpollInfo *epoll_info = init_epoll_info(sock_fd);  
  set_accept_epoll_event(epoll_info);

  for (int i = 0 ; i < server->actor_count ; i++) {
    
  }
  
  while (1) {
    //print_stats(event_loop);
    struct epoll_event events[MAX_EVENTS];    
    int ready_amount = epoll_wait(epoll_info->epoll_fd, events, MAX_EVENTS, -1);
    CHECK(ready_amount == -1, "failed waiting on epoll");

    LOG_DEBUG("epoll events %d", ready_amount);
    
    for (int i = 0 ; i < ready_amount ; i++) {
      if (handle_poll_errors(epoll_info, &events[i]) != 0) {
	continue;
      }
      if (events[i].data.ptr == epoll_info) {
	RequestContext *request_context = process_accept(epoll_info);
	int fd = request_context->fd;
	input_epoll_event(epoll_info, fd, request_context);
	continue;
      }
      RequestContext *request_context = (RequestContext*) events[i].data.ptr;
      
      if (events[i].events & EPOLLIN && request_context->state == CLIENT_READ) {
	read_client_request(epoll_info, server, request_context);
      }

      if (events[i].events & EPOLLOUT && request_context->state == CLIENT_WRITE) {
	write_client_response(epoll_info, request_context);
      }

      if (events[i].events & EPOLLOUT && request_context->state == ACTOR_WRITE) {
	write_actor_request(epoll_info, request_context);
      }

      if (events[i].events & EPOLLIN && request_context->state == ACTOR_READ) {
	read_actor_response(epoll_info, request_context);
      }
    }
  }  
}

