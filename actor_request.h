#ifndef __actor_request_h__
#define __actor_request_h__

#include "http_request.h"

typedef struct ActorRequest {
  HttpRequest *http_request;
} ActorRequest;

ActorRequest *init_actor_request(HttpRequest *http_request);

#endif
