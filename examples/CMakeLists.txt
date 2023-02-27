# executable
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(example main.c)

target_link_libraries(example PRIVATE snot)

configure_file(example1.snot example1.snot COPYONLY)
configure_file(example2.snot example2.snot COPYONLY)
configure_file(example3.snot example3.snot COPYONLY)
configure_file(example4.snot example4.snot COPYONLY)
configure_file(example5.snot example5.snot COPYONLY)