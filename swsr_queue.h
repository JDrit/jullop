#ifndef __swsr_queue_h__
#define __swsr_queue_h__

typedef struct SwsrQueue {

} SwsrQueue;


SwsrQueue swsr_queue_init(size_t max_size);


void swsr_queue_destroy(SwsrQueue *queue);


size_t swsr_queue_size(Queue *queue);

void swsr_queue_push(Queue *queue, void *data);

enum QueueResult swsr_queue_trypush(Queue *queue, void *data);

void swsr_queue_pop(Queue *queue, void **data);

enum QueueResult swsr_queue_poptry(Queue *queue, void **data);

#endif
