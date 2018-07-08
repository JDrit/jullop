#ifndef __message_passing_h__
#define __message_passing_h__

#include <stddef.h>

/**
 * This is used for passing messages around actors. To keep the amount of data
 * going over the sockets small, the messages only contain a pointer address
 * where the full message is stored.
 *
 * This requires that before the receiver starts using the payload in the
 * message they must first use a memory fence. 
 *
 * Also as soon as the sender starts writing data they must not touch the
 * payload again to prevent against race conditions.
 */

enum ReadState {
  READ_FINISH,
  READ_BUSY,
  READ_ERROR,
  CLIENT_DISCONNECT
};

enum WriteState {
  WRITE_FINISH,
  WRITE_BUSY,
  WRITE_ERROR
};

typedef struct Message {
  char buffer[sizeof(size_t)];
  size_t offset;
} Message;

/**
 * Creates a message that can be used to pass a reference through
 * a socket.
 */
void message_init(Message *message, void *ptr);

/**
 * Clears the data stored in the given message.
 */
void message_reset(Message *message);

/**
 * Once the message has been fully read in, this can be called to
 * get the value that was passed.
 */
void *message_get_payload(Message *message);

/**
 * Fully writes the given message out to the given file descriptor.
 */
void message_write_sync(int fd, Message *message);

/**
 * Fully reads in a message from the given file descriptor, blocking
 * till ready. It modifies the given argument to include the new
 * message.
 */
void message_read_sync(int fd, Message *message);

/**
 * Attempts to write a message out to the given socket. The given
 * message can be partially written and it will continue to pick
 * up where it left off.
 *
 * The return value indicates if there is more to be written or not.
 */
enum WriteState message_write_async(int fd, Message *message);

/**
 * Attempts to read a message from the given socket. the given
 * message can be partially read and it will continue to pick
 * up where it left off.
 *
 * the return value indicates if there is more to be read or not.
 */
enum ReadState message_read_async(int fd, Message *message);

/**
 * Attempts to write out the buffer to a socket on the given file descriptor.
 * It will attempt to write out buffer_len amount of byes.
 *
 * This allows write requests to be resumed by keeping track of the offset.
 * The offset is modified after the function call to point to the next location
 * to start writing fro,.
 */
enum WriteState write_async(int fd, void *buffer, size_t buffer_len, size_t *offset);

/**
 * Attemps to fill the given buffer with data off of a socket on the given file
 * descriptor. It will fill at most buffer_len.
 *
 * The offset is used to allow for resumable reading. After this function call,
 * offset will be modified with the new location to start reading from.
 */
enum ReadState read_async(int fd, void *buffer, size_t buffer_len, size_t *offset);

#endif
