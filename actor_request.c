#include <stdlib.h>

#include "actor_request.h"
#include "http_request.h"
#include "logging.h"

ActorRequest *init_actor_request(HttpRequest *http_request) {
  ActorRequest *request = CHECK_MEM(calloc(1, sizeof(ActorRequest)));
  request->http_request = http_request;  
  return request;
}
