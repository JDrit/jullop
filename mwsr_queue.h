
typedef struct MwsrQueue {
  size_t max_size;
  size_t current_size;
  void **ring_buffer;
  size_t enqueue_offset;
  size_t dequeue_offset;
  
}
