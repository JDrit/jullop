#ifndef __client_h__
#define __client_h__

#include <stdbool.h>

#include "epoll_info.h"
#include "io_worker.h"
#include "request_context.h"
#include "server.h"

/**
 * Tries to read as much as needed off of the active connection. Returns true if 
 * the entire request was read, false if epoll needs to be used to get finish 
 * reading the request.
 */
bool client_read_request(Server *server, EpollInfo *epoll_info, int id,
			 IoContext *io_context);

/**
 * Writes out as much as possible out to the active connection. Returns true if
 * the entire response was written out, false if epoll needs to be used to 
 * finish writing out the response.
 */
bool client_write_response(Server *server, EpollInfo *epoll_info,
			   IoContext *io_context);

/**
 * Starts the connection pipeline from the beginning.
 */
void client_reset_connection(Server *server, EpollInfo *epoll_info,
			     IoContext *io_context, enum RequestResult result);

/**
 * Closes the client's connection and frees all resources from it.
 */
void client_close_connection(Server *server, EpollInfo *epoll_info,
			     IoContext *io_context, enum RequestResult result);

#endif
