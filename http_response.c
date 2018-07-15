#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "http_response.h"
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

static inline size_t int_size(int i) {
  if (i < 10)
    return 1;
  if (i < 100)
    return 2;
  if (i < 1000)
    return 3;
  if (i < 10000)
    return 4;
  if (i < 100000)
    return 5;
  if (i < 1000000)
    return 6;      
  if (i < 10000000)
    return 7;
  if (i < 100000000)
    return 8;
  if (i < 1000000000)
    return 9;
  return 10;
}

void http_response_init(HttpResponse *http_response, int status_code,
			HttpHeader headers[10], size_t header_count,
			const char *body, size_t body_len) {

  const char *reason_phrase = get_reason(status_code);
  size_t status_size = STATUS_SIZE + int_size(status_code) + strlen(reason_phrase);
  
  size_t header_size = 0;
  for (size_t i = 0 ; i < header_count ; i++) {
    // 4 is for the overhead for each header
    header_size += headers[i].name_len + headers[i].value_len + 4;
  }

  size_t total_size = status_size + header_size + BREAK_SIZE + body_len;
  char *output = (char*) CHECK_MEM(calloc(total_size, sizeof(total_size)));
  

  int offset = 0;
  offset += sprintf(output, "HTTP/1.1 %d %s\r\n", status_code, reason_phrase);

  for (size_t i = 0 ; i < header_count ; i++) {
    offset += sprintf(output + offset, "%.*s: %.*s\r\n",
		      (int) headers[i].name_len, headers[i].name,
		      (int) headers[i].value_len, headers[i].value);
  }

  offset += sprintf(output + offset, "\r\n%.*s", (int) body_len, body);

  http_response->output = output;
  http_response->output_len = (size_t) offset;

}
