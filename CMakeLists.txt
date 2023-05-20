cmake_minimum_required(VERSION 3.23)
project(Task3 C)

set(CMAKE_C_STANDARD 99)

add_executable(Task3 main.c)
add_executable(04_05_basic_server 04_05_basic/server.c 04_05_basic/params.h 04_05_basic/utils.h)
add_executable(04_05_basic_garden 04_05_basic/garden.c 04_05_basic/params.h 04_05_basic/utils.h)
add_executable(04_05_basic_gardener 04_05_basic/gardener.c 04_05_basic/params.h 04_05_basic/utils.h)
add_executable(06_07_display_server 06_07_display/server.c 06_07_display/params.h 06_07_display/utils.h)
add_executable(06_07_display_garden 06_07_display/garden.c 06_07_display/params.h 06_07_display/utils.h)
add_executable(06_07_display_gardener 06_07_display/gardener.c 06_07_display/params.h 06_07_display/utils.h)
