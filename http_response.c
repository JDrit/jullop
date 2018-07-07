#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"

#define STATUS_SIZE 12
#define CONTENT_SIZE 18
#define BREAK_SIZE 2

static inline char *get_reason(int status_code) {
  switch (status_code) {
  case 100: return "Continue";
  case 101: return "Switching Protocols";
  case 200: return "Ok";
  case 201: return "Created";
  case 202: return "Accepted";
  case 203: return "Non-Authoritative Information";
  case 204: return "No Content";
  case 205: return "Reset Content";
  case 400: return "Bad  Request";
  case 401: return "Unauthorized";
  case 402: return "Payment Required";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 405: return "Method Not Allowed";
  case 406: return "Not Acceptable";
  case 500: return "Internal Server Error";
  default: return "Other";
  }      
}

static inline size_t str_size(size_t i) {
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

char *init_http_response(int status_code, char *body, size_t body_len) {
  char *reason_phrase = get_reason(status_code);
  size_t size =
    STATUS_SIZE + str_size((size_t) status_code) + strlen(reason_phrase) +
    CONTENT_SIZE + str_size(body_len) +
    BREAK_SIZE +
    body_len;
  char *buf = (char*) CHECK_MEM(calloc(size, sizeof(char)));
  sprintf(buf,
	  "Http/1.1 %d %s\r\n"
	  "Content-Length: %zd\r\n"
	  "\r\n"
	  "%.*s",
	  status_code, reason_phrase,
	  body_len,
	  (int) body_len, body);
  return buf;
}
			 
