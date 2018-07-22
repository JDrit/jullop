add_executable(input_buffer check_input_buffer.c)
target_link_libraries(input_buffer jullop check)
add_test(input_buffer_test input_buffer)

add_executable(output_buffer check_output_buffer.c)
target_link_libraries(output_buffer jullop check)
add_test(output_buffer_test output_buffer)
