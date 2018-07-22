#define _GNU_SOURCE

#include <check.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../src/input_buffer.h"
#include "../src/logging.h"

static void create_sockets(int fds[2]) {
  int r = socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, fds);
  CHECK(r != 0, "Failed to create actor socket pair");

  r = fcntl(fds[0], F_SETFL, O_NONBLOCK);
  CHECK(r != 0, "Failed to set non-blocking");

  r = fcntl(fds[1], F_SETFL, O_NONBLOCK);
  CHECK(r != 0, "Failed to set non-blocking");
}

START_TEST(input_buffer_read) {
  InputBuffer *buffer = input_buffer_init(1024);
  
  int fds[2];
  create_sockets(fds);
  int input = fds[0];
  int output = fds[1];
  
  dprintf(input, "testing 1 2 3");
  enum ReadState state = input_buffer_read_into(buffer, output);
  
  ck_assert_int_eq(state, READ_BUSY);
  ck_assert_str_eq(buffer->buffer, "testing 1 2 3");
  ck_assert_int_eq(buffer->offset, 13);
  
  input_buffer_destroy(buffer);
} END_TEST

START_TEST(input_buffer_mult_reads) {
  InputBuffer *buffer = input_buffer_init(1024);
  int fds[2];
  create_sockets(fds);
  int input = fds[0];
  int output = fds[1];

  dprintf(input, "testing-%d", 1);
  input_buffer_read_into(buffer, output);
  ck_assert_str_eq(buffer->buffer, "testing-1");

  dprintf(input, "testing-%d", 2);
  input_buffer_read_into(buffer, output);
  ck_assert_str_eq(buffer->buffer, "testing-1testing-2");

  input_buffer_destroy(buffer);
} END_TEST;

START_TEST(input_buffer_resize) {
  InputBuffer *buffer = input_buffer_init(2);
  int fds[2];
  create_sockets(fds);
  int input = fds[0];
  int output = fds[1];

  const char *str = "testing-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15";
  dprintf(input, "%s", str);
  input_buffer_read_into(buffer, output);

  ck_assert_str_eq(buffer->buffer, str);
  ck_assert_int_eq(buffer->length, 20);

  input_buffer_destroy(buffer);
} END_TEST;

Suite *input_buffer_suite(void) {
  Suite *suite = suite_create("input buffer");
  TCase *tc_core = tcase_create("Core");

  //tcase_add_test(tc_core, input_buffer_read);
  //tcase_add_test(tc_core, input_buffer_mult_reads);
  tcase_add_test(tc_core, input_buffer_resize);
  suite_add_tcase(suite, tc_core);

  return suite;
}

int main(void) {
  Suite *suite = input_buffer_suite();
  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  int number_failed = srunner_ntests_failed(runner);

  srunner_free(runner);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
