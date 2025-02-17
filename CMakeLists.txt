cmake_minimum_required(VERSION 3.30)
project(c_roguelike_framework C)
set(CMAKE_C_STANDARD 99)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(third-party/SDL EXCLUDE_FROM_ALL)

#add_compile_options(-Wall -Werror -Wpedantic)
#add_compile_options(-Wextra -Wconversion)

add_executable(c_roguelike_framework main.c)

target_link_libraries(c_roguelike_framework PRIVATE SDL3::SDL3)