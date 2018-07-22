#define _GNU_SOURCE

#include <check.h>
#include <errno.h>
#include <stdlib.h>

#include "../src/output_buffer.h"

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

START_TEST(output_buffer_write_to_resize) {
  errno = 0;
  OutputBuffer *buffer = output_buffer_init(2);

  output_buffer_append(buffer, "testing %d %d %d", 5, 6, 7);
  ck_assert_str_eq(buffer->buffer, "testing 5 6 7");
  
  output_buffer_destroy(buffer);
} END_TEST

START_TEST(output_buffer_reuse) {

} END_TEST;

Suite *output_buffer_suite(void) {
  Suite *suite = suite_create("output buffer");
  TCase *tc_core = tcase_create("Core");

  tcase_add_test(tc_core, output_buffer_write_to_buffer);
  tcase_add_test(tc_core, output_buffer_write_to_vargs);
  tcase_add_test(tc_core, output_buffer_write_to_resize);
  tcase_add_test(tc_core, output_buffer_reuse);

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
  
