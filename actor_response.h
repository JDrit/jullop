#ifndef __actor_response_h__
#define __actor_response_h__

typedef struct ActorResponse {
  char *buffer;
  size_t length;
} ActorResponse;

ActorResponse *init_actor_response(char *buffer, size_t length);

#endif
