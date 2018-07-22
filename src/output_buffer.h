#ifndef __output_buffer_h__
#define __output_buffer_h__

enum WriteState {
  WRITE_FINISH,
  WRITE_BUSY,
  WRITE_ERROR
};

typedef struct OutputBuffer {
  /* the pointer to the head of the buffer. This should not be directly
   * accessed by clients. */
  char *buffer;
  
  /* stores the total size of this buffer. */
  size_t length;

  /* stores the offset when writing out of this buffer. */
  size_t write_from_offset;

  /* stores the offset when writing into this buffer. */
  size_t write_into_offset;

  /* the number of times the buffer was resized. */
  size_t resize_count;

} OutputBuffer;

/**
 * Constructs a buffer used to write data to the client that can store
 * an initial size of the given argument.
 */
OutputBuffer *output_buffer_init(size_t size);

/**
 * Frees up all memory associated with this buffer.
 */
void output_buffer_destroy(OutputBuffer *buffer);

/**
 * Clears out the output buffer so that it can be used again.
 */
void output_buffer_reset(OutputBuffer *buffer);

/**
 * Attempts to write as much as possible of this buffer to a socket described
 * by the given file descriptor. The return value indicates if there is more
 * work to do or not.
 */
enum WriteState output_buffer_write_to(OutputBuffer *buffer, int fd);

/**
 * Writes the given format string and its given arguments to the 
 * output buffer, extending the underlying data structure if needed
 * along the way.
 */
void output_buffer_append(OutputBuffer *buffer, const char *fmt, ...)
  __attribute__ ((format (printf, 2, 3)));

#endif
