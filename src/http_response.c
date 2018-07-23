#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "http_response.h"
#include "output_buffer.h"
#include "logging.h"

#define STATUS_SIZE 12
#define CONTENT_SIZE 18
#define CONNECTION_SIZE 24
#define BREAK_SIZE 2

static inline const char *get_reason(int status_code) {
  switch (status_code) {

  // 1xx: Informational - Request received, continuing process
  case 100: return "Continue";
  case 101: return "Switching Protocols";

  // 2xx: Success - The action was successfully received, understood, and accepted
  case 200: return "Ok";
  case 201: return "Created";
  case 202: return "Accepted";
  case 203: return "Non-Authoritative Information";
  case 204: return "No Content";
  case 205: return "Reset Content";
  case 206: return "Partial Content";

  // 3xx: Redirection - Further action must be taken in order to complete the request
  case 300: return "Multiple Choices";
  case 301: return "Moved Permanently";
  case 302: return "Found";
  case 303: return "See Other";
  case 304: return "Not Modified";
  case 305: return "Use Proxy";
  case 307: return "Temporary Redirect";

  // 4xx: Client Error - The request contains bad syntax or cannot be fulfilled
  case 400: return "Bad Request";
  case 401: return "Unauthorized";
  case 402: return "Payment Required";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 405: return "Method Not Allowed";
  case 406: return "Not Acceptable";
  case 407: return "Proxy Authentication Required";
  case 408: return "Request Time-out";
  case 409: return "Conflict";
  case 410: return "Gone";

  // 5xx: Server Error - The server failed to fulfill an apparently valid request  
  case 500: return "Internal Server Error";
  case 501: return "Not Implemented";
  case 502: return "Bad Gateway";
  case 503: return "Service Unavailable";
  case 504: return "Gateway Time-out";
  case 505: return "HTTP Version not supported";
  default: return "Other";
  }      
}

void http_response_init(OutputBuffer *buffer, int status_code,
			HttpHeader headers[10], size_t header_count,
			const char *body, size_t body_len) {

  const char *reason_phrase = get_reason(status_code);
  output_buffer_append(buffer, "HTTP/1.1 %d %s\r\n", status_code, reason_phrase);

  for (size_t i = 0 ; i < header_count ; i++) {
    output_buffer_append(buffer, "%.*s: %.*s\r\n",
			 (int) headers[i].name_len, headers[i].name,
			 (int) headers[i].value_len, headers[i].value);
  }
  output_buffer_append(buffer, "\r\n%.*s", (int) body_len, body);
}
