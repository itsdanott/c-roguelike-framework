cmake_minimum_required(VERSION 3.26)
project(c_roguelike_framework C)
set(CMAKE_C_STANDARD 99)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
if(EMSCRIPTEN)
    set(OUT_DIR "${CMAKE_BINARY_DIR}/../bin-Emscripten")
else()
    set(OUT_DIR "${CMAKE_BINARY_DIR}/../bin-$<CONFIGURATION>")
endif ()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}")

add_subdirectory(third-party/SDL EXCLUDE_FROM_ALL)

# on Web targets, we need CMake to generate a HTML webpage.
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

add_subdirectory(third-party/glad-3.3core-3.0es)

add_executable(c_roguelike_framework
    main.c
    third-party/stb/stb_truetype.h
    third-party/stb/stb_image.h
)

#target_compile_options(c_roguelike_framework PRIVATE
#        -Wall
#        -Werror
#        -Wextra
#        -Wconversion
#        -Wpedantic
#        -Werror=vla
#)

target_link_libraries(c_roguelike_framework PRIVATE
        SDL3::SDL3
        glad
)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(c_roguelike_framework PRIVATE -static -pthread)
endif()

if(EMSCRIPTEN)
    set_target_properties(c_roguelike_framework PROPERTIES
        SUFFIX ".html"
    )
    set(EMSCRIPTEN_LINK_FLAGS
        "-sMAX_WEBGL_VERSION=2"
        "--shell-file=${CMAKE_SOURCE_DIR}/third-party/emscripten/shell_minimal_modified.html"
        "--use-preload-plugins"
        "--preload-file=${CMAKE_SOURCE_DIR}/assets@/assets"
        "-sSTACK_SIZE=1000000" # 1 MB Stack size
    )
    string(JOIN " " LINK_FLAGS_STR ${EMSCRIPTEN_LINK_FLAGS})

    set_target_properties(c_roguelike_framework PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")

endif()