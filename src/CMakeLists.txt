add_library(lock_queue STATIC mrmw_queue.c)
target_compile_options(lock_queue PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics)

add_library(atomic_queue STATIC srsw_queue.c)
target_compile_options(atomic_queue PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics)

add_library(jullop STATIC
  actor.c
  epoll_info.c
  http_request.c
  http_response.c
  input_buffer.c
  io_worker.c
  message_passing.c
  output_buffer.c
  picohttpparser.c
  request_context.c
  time_stats.c)
target_compile_options(jullop PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics -DDEBUG)
target_link_libraries(jullop pthread tcmalloc lock_queue)

add_executable(main main.c)
target_compile_options(main PRIVATE
  -std=gnu11 -g -O0 -Wall -Wextra -Wconversion -fno-builtin-malloc
  -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
  -fcolor-diagnostics)
target_link_libraries(main jullop)
