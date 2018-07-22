#define _GNU_SOURCE

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../src/logging.h"
#include "../src/output_buffer.h"

static void create_sockets(int fds[2]) {
  int r = socketpair(AF_LOCAL, SOCK_STREAM, 0, fds);
  CHECK(r != 0, "Failed to create actor socket pair");

  r = fcntl(fds[0], F_SETFL, O_NONBLOCK);
  CHECK(r != 0, "Failed to set non-blocking");

  r = fcntl(fds[1], F_SETFL, O_NONBLOCK);
  CHECK(r != 0, "Failed to set non-blocking");
}

START_TEST(output_buffer_write_to_buffer) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(1024);
  
  output_buffer_append(buffer, "testing");
  ck_assert_str_eq(buffer->buffer, "testing");

  output_buffer_append(buffer, " testing");
  ck_assert_str_eq(buffer->buffer, "testing testing");

  ck_assert_int_eq(buffer->write_into_offset, 15); 

  output_buffer_destroy(buffer);
} END_TEST

START_TEST(output_buffer_write_to_vargs) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(1024);

  output_buffer_append(buffer, "testing %d %d %d", 1, 2, 3);
  ck_assert_str_eq(buffer->buffer, "testing 1 2 3");

  output_buffer_destroy(buffer);
} END_TEST

START_TEST(output_buffer_resize) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(2);

  output_buffer_append(buffer, "testing");
  ck_assert_str_eq(buffer->buffer, "testing");
  ck_assert_int_eq(buffer->resize_count, 1);

  output_buffer_append(buffer, " happy happy");
  ck_assert_str_eq(buffer->buffer, "testing happy happy");

  output_buffer_append(buffer, " this is a test of the emergency broadcast message");
  ck_assert_str_eq(buffer->buffer,
		   "testing happy happy this is a test of the emergency broadcast message");
  ck_assert_int_eq(buffer->resize_count, 3);
    
  output_buffer_destroy(buffer);
} END_TEST

START_TEST(output_buffer_resize_vargs) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(2);

  output_buffer_append(buffer, "testing %d", 5);
  ck_assert_str_eq(buffer->buffer, "testing 5");
  ck_assert_int_eq(buffer->resize_count, 1);
  
  output_buffer_destroy(buffer);
} END_TEST

START_TEST(output_buffer_reuse) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(102);

  output_buffer_append(buffer, "testing 1 2 3 4 5 6 7 8");
  output_buffer_reset(buffer);
  ck_assert_int_eq(buffer->write_into_offset, 0);

  output_buffer_append(buffer, "fake fake");
  ck_assert(strncmp(buffer->buffer, "fake fake", strlen("fake fake")) == 0);
  
  output_buffer_destroy(buffer);
} END_TEST

START_TEST(output_buffer_write) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(1024);

  int fds[2];
  create_sockets(fds);
  int input = fds[0];
  int output = fds[1];

  enum WriteState state;

  output_buffer_append(buffer, "writing to socket");
  state = output_buffer_write_to(buffer, input);
  ck_assert_int_eq(state, WRITE_FINISH);
  ck_assert_int_eq(buffer->write_from_offset, 17);

  output_buffer_append(buffer, " 1 2 3");
  state = output_buffer_write_to(buffer, input);
  ck_assert_int_eq(state, WRITE_FINISH);

  state = output_buffer_write_to(buffer, input);
  ck_assert_int_eq(state, WRITE_FINISH);
    
  char buf[256];
  read(output, buf, 256);
  ck_assert_str_eq(buf, "writing to socket 1 2 3");

  output_buffer_append(buffer, "4 5 6");
  state = output_buffer_write_to(buffer, input);
  ck_assert_int_eq(state, WRITE_FINISH);

  char buf2[256];
  read(output, buf2, 256);
  ck_assert(strncmp(buf2, "4 5 6", strlen("4 5 6")) == 0);

  output_buffer_destroy(buffer);
} END_TEST

Suite *output_buffer_suite(void) {
  Suite *suite = suite_create("output buffer");
  TCase *tc_core = tcase_create("Core");

  tcase_add_test(tc_core, output_buffer_write_to_buffer);
  tcase_add_test(tc_core, output_buffer_write_to_vargs);
  tcase_add_test(tc_core, output_buffer_resize);
  tcase_add_test(tc_core, output_buffer_resize_vargs);
  tcase_add_test(tc_core, output_buffer_reuse);
  tcase_add_test(tc_core, output_buffer_write);

  suite_add_tcase(suite, tc_core);
  return suite;
}

int main(void) {
  Suite *suite = output_buffer_suite();
  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  int number_failed = srunner_ntests_failed(runner);
  
  srunner_free(runner);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
  
