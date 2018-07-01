#ifndef __http_request_h__
#define __http_request_h__

#include "picohttpparser.h"

typedef struct HttpRequest {
  const char *method;
  size_t method_len;

  const char *path;
  size_t path_len;

  int minor_version;

  struct phr_header headers[100];
  size_t num_headers;
  
} HttpRequest;

enum ParseState {
  PARSE_FINISH,
  PARSE_ERROR,
  PARSE_INCOMPLETE,
};

/**
 * Tries to parse out an HTTP request from the given buffer.
 *  Returns a enumeration detailing if there is more work to do or not.
 */
enum ParseState http_parse(char* buffer, size_t buf_len, size_t prev_len,
			   HttpRequest *request);

void http_request_print(HttpRequest *request);

#endif
