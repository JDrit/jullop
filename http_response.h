#ifndef __http_response_h__
#define __http_response_h__

typedef struct HttpResponse {
  char *output;
  size_t output_len;
} HttpResponse;

/**
 * Creates a HTTP response that uses the given status code for the HTTP
 * result code and the body as the payload to the response.
 *
 * It modifies the passed in http_response to include a pointer to an
 * allocated piece of memory. The caller is responsible for cleaning
 * up the response's output.
 */
void init_http_response(HttpResponse *http_response, int status_code,
			const char *body, size_t body_len);

#endif
