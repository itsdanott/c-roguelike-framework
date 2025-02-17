cmake_minimum_required(VERSION 3.26)
project(c_roguelike_framework C)
set(CMAKE_C_STANDARD 99)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(third-party/SDL EXCLUDE_FROM_ALL)

# we just need basic text rendering - only FREETYPE required.
set(SDLTTF_VENDORED ON)
set(SDLTTF_HARFBUZZ OFF)
set(SDLTTF_PLUTOSVG OFF)
add_subdirectory(third-party/SDL_ttf EXCLUDE_FROM_ALL)

# on Web targets, we need CMake to generate a HTML webpage.
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

#add_compile_options(-Wall -Werror -Wpedantic)
#add_compile_options(-Wextra -Wconversion)

add_executable(c_roguelike_framework
    main.c
        tiny_ttf.h
)

target_link_libraries(c_roguelike_framework PRIVATE SDL3_ttf::SDL3_ttf SDL3::SDL3)

if(EMSCRIPTEN)
    set_target_properties(c_roguelike_framework PROPERTIES
        SUFFIX ".html"
    )
    set_target_properties(c_roguelike_framework PROPERTIES LINK_FLAGS
        "--shell-file ${CMAKE_SOURCE_DIR}/third-party/emscripten/shell_minimal_modified.html"
    )
endif()