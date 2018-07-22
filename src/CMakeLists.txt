add_library(jullop STATIC
  actor.c
  epoll_info.c
  http_request.c
  http_response.c
  input_buffer.c
  io_worker.c
  message_passing.c
  mrmw_queue.c
  output_buffer.c
  picohttpparser.c
  request_context.c
  srsw_queue.c
  time_stats.c)

target_compile_options(jullop PRIVATE -std=gnu11 -c -g -O0 -Wall -Wextra -Wconversion)
target_compile_options(jullop PRIVATE -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc)
target_compile_options(jullop PRIVATE -fno-builtin-free -DDEBUG)
target_compile_options(jullop PRIVATE -DLOCK_QUEUE)
target_link_libraries(jullop pthread tcmalloc)

add_executable(main main.c)
target_compile_options(main PRIVATE -std=gnu11 -c -g -O0 -Wall -Wextra -Wconversion)
target_compile_options(main PRIVATE -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc)
target_compile_options(main PRIVATE -fno-builtin-free -DDEBUG)
target_compile_options(main PRIVATE -DLOCK_QUEUE)
target_link_libraries(main jullop)
