#ifndef __client_h__
#define __client_h__

#include "epoll_info.h"
#include "io_worker.h"
#include "request_context.h"
#include "server.h"

/**
 * Tries to read as much as needed off of the active connection. Returns true if 
 * the entire request was read, false if epoll needs to be used to get finish 
 * reading the request.
 */
void client_handle_read(SocketContext *context);

/**
 * Writes out as much as possible out to the active connection. Returns true if
 * the entire response was written out, false if epoll needs to be used to 
 * finish writing out the response.
 */
void client_handle_write(SocketContext *context);

void client_handle_error(SocketContext *context, uint32_t events);

/**
 * Starts the connection pipeline from the beginning.
 */
void client_reset_connection(SocketContext *context, enum RequestResult result);

/**
 * Closes the client's connection and frees all resources from it.
 */
void client_close_connection(SocketContext *context, enum RequestResult result);

#endif
