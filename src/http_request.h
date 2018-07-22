#ifndef __http_request_h__
#define __http_request_h__

#include "input_buffer.h"
#include "picohttpparser.h"

#define NUM_HEADERS 100

typedef struct HttpRequest {
  const char *method;
  size_t method_len;

  const char *path;
  size_t path_len;

  int minor_version;

  struct phr_header headers[NUM_HEADERS];
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
enum ParseState http_request_parse(InputBuffer *buffer, size_t prev_len,
				   HttpRequest *request);

void http_request_print(HttpRequest *request);

#endif
