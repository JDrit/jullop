#include <stdlib.h>

#include "actor_response.h"
#include "logging.h"

ActorResponse *init_actor_response(char *buffer, size_t length) {
  ActorResponse *response = CHECK_MEM(calloc(1, sizeof(ActorResponse)));
  response->buffer = buffer;
  response->length = length;

  return response;
}
