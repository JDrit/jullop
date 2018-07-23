add_executable(input_buffer check_input_buffer.c)
target_compile_options(input_buffer PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics)
target_link_libraries(input_buffer jullop check)
add_test(input_buffer_test input_buffer)

add_executable(output_buffer check_output_buffer.c)
target_compile_options(output_buffer PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics)
target_link_libraries(output_buffer jullop check)
add_test(output_buffer_test output_buffer)

add_executable(lock_queue_ex check_lock_queue.c)
target_compile_options(lock_queue_ex PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics)
target_link_libraries(lock_queue_ex lock_queue jullop check)
add_test(lock_queue_test lock_queue_ex)
