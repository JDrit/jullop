#ifndef __http_response_h__
#define __http_response_h__

char *init_http_response(int status_code, char *body, size_t body_len);

#endif
