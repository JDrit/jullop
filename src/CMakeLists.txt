add_library(lock_queue STATIC mrmw_queue.c)
target_compile_options(lock_queue PRIVATE
  -std=gnu11 -g -O3 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics -Wno-unused-parameter)

add_library(atomic_queue STATIC srsw_queue.c)
target_compile_options(atomic_queue PRIVATE
  -std=gnu11 -g -O3 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics -Wno-unused-parameter)

add_library(jullop STATIC
  actor.c
  client.c
  epoll_info.c
  http_request.c
  http_response.c
  input_buffer.c
  io_worker.c
  message_passing.c
  output_buffer.c
  picohttpparser.c
  request_context.c
  stats.c
  stats_thread.c
  time_stats.c)
target_compile_options(jullop PRIVATE
  -std=gnu11 -g -O3 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics -Wno-unused-parameter)
target_link_libraries(jullop pthread lock_queue)

add_executable(main main.c)
target_compile_options(main PRIVATE
  -std=gnu11 -g -O3 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics -Wno-unused-parameter)
target_link_libraries(main jullop)
