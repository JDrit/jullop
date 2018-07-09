#define _GNU_SOURCE

#include <stdlib.h>

#include "http_request.h"
#include "logging.h"
#include "picohttpparser.h"


enum ParseState http_request_parse(char* buffer, size_t buf_len, size_t prev_len,
				   HttpRequest *request) {
    int result = phr_parse_request(buffer, buf_len,
				 &request->method, &request->method_len,
				 &request->path, &request->path_len,
				 &request->minor_version,
				 request->headers, &request->num_headers,
				 prev_len);

  switch (result) {
  case -1:
    return PARSE_ERROR;
  case -2:
    return PARSE_INCOMPLETE;
  default:
    return PARSE_FINISH;
  }
}

void http_request_print(HttpRequest *request) {
  LOG_INFO("method  : %.*s", (int) request->method_len, request->method);
  LOG_INFO("path    : %.*s", (int) request->path_len, request->path);
  LOG_INFO("version : 1.%d", request->minor_version);
  LOG_INFO("headers :");
  for (size_t i = 0 ; i < request->num_headers ; i++) {
    LOG_INFO("\t%.*s = %.*s",
	     (int) request->headers[i].name_len, request->headers[i].name,
	     (int) request->headers[i].value_len, request->headers[i].value);
  }

}
