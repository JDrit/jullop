#define _GNU_SOURCE

#include <check.h>
#include <sys/eventfd.h>
#include <stdlib.h>

#include "../src/logging.h"
#include "../src/queue.h"

START_TEST(queue_push_pop) {
  Queue *queue = queue_init(10);

  queue_push(queue, (void*) 5);
  queue_push(queue, (void*) 6);
  queue_push(queue, (void*) 7);

  ck_assert_int_eq(5, (int) queue_pop(queue));
  ck_assert_int_eq(6, (int) queue_pop(queue));
  ck_assert_int_eq(7, (int) queue_pop(queue));

  queue_destroy(queue);
} END_TEST

START_TEST(queue_get_size) {
  errno = 0;
  Queue *queue = queue_init(10);

  queue_push(queue, (void*) 1);
  ck_assert_int_eq(1, queue_size(queue));

  queue_push(queue, (void*) 1);
  ck_assert_int_eq(2, queue_size(queue));

  queue_pop(queue);
  ck_assert_int_eq(1, queue_size(queue));

  queue_pop(queue);
  ck_assert_int_eq(0, queue_size(queue));

  queue_destroy(queue);
} END_TEST

START_TEST(queue_too_small) {
  errno = 0;
  Queue *queue = queue_init(1);

  enum QueueResult result = queue_push(queue, (void*) 1);
  ck_assert_int_eq(result, QUEUE_SUCCESS);

  result = queue_push(queue, (void*) 2);
  ck_assert_int_eq(result, QUEUE_FAILURE);

  ck_assert_int_eq((int) queue_pop(queue), 1);

  result = queue_push(queue, (void*) 3);
  ck_assert_int_eq(result, QUEUE_SUCCESS);

  ck_assert_int_eq((int) queue_pop(queue), 3);
  
  queue_destroy(queue);
} END_TEST

START_TEST(queue_event_fd) {
  errno = 0;
  Queue *queue = queue_init(5);

  queue_push(queue, (void*) 1);
  queue_push(queue, (void*) 2);
  queue_push(queue, (void*) 3);

  eventfd_t val;
  eventfd_read(queue_add_event_fd(queue), &val);
  ck_assert_int_eq(3, val);

  int r = eventfd_read(queue_add_event_fd(queue), &val);
  ck_assert_int_eq(r, -1);
  ck_assert_int_eq(errno, EAGAIN);
  
  queue_destroy(queue);
} END_TEST

Suite *queue_suite(void) {
  Suite *suite = suite_create("lock queue suite");
  TCase *tc_core = tcase_create("Core");

  tcase_add_test(tc_core, queue_push_pop);
  tcase_add_test(tc_core, queue_get_size);
  tcase_add_test(tc_core, queue_too_small);
  tcase_add_test(tc_core, queue_event_fd);
  suite_add_tcase(suite, tc_core);
  return suite;
}

int main(void) {
  Suite *suite = queue_suite();
  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_NORMAL);
  int number_failed = srunner_ntests_failed(runner);
  
  srunner_free(runner);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
