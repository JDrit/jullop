add_executable(tests check_input_buffer.c)

target_link_libraries(tests jullop check)
add_test(input_buffer_test tests)
