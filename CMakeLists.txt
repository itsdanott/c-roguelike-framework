cmake_minimum_required(VERSION 3.30)
project(c_roguelike_framework C)

add_compile_options(-Wall -Werror -Wpedantic)
add_compile_options(-Wextra -Wconversion)


set(CMAKE_C_STANDARD 99)

add_executable(c_roguelike_framework main.c)
