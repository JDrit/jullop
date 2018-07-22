#ifndef __actor_h__
#define __actor_h__

struct ActorArgs;
typedef struct ActorArgs ActorArgs;

ActorArgs *init_actor_args(int id, int fd);

void  *run_actor(void *args);

#endif
