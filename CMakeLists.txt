cmake_minimum_required(VERSION 3.26)
project(c_roguelike_framework C)
set(CMAKE_C_STANDARD 99)

# OUTPUT DIRECTORIES ***********************************************************
if (EMSCRIPTEN)
    set(OUT_DIR "${CMAKE_BINARY_DIR}/../bin-Emscripten")
else ()
    set(OUT_DIR "${CMAKE_BINARY_DIR}/../bin-$<CONFIGURATION>")
endif ()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}")

# SUB CMAKE DIRECTORIES ********************************************************
add_subdirectory(third-party/SDL EXCLUDE_FROM_ALL)
add_subdirectory(third-party/glad-3.3core-3.0es)
add_subdirectory(game)

# HTML *************************************************************************
if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif ()

# EXECUTABLE *******************************************************************
add_executable(c_roguelike_framework
        c_roguelike_framework.c

        c_roguelike_framework.h
        third-party/stb/stb_truetype.h
        third-party/stb/stb_image.h
)
# TARGET **********************************************************************
set_target_properties(c_roguelike_framework PROPERTIES LINKER_LANGUAGE C)
target_compile_options(c_roguelike_framework PRIVATE
        #        -fsanitize=address
        #        -Wall
        #        -Werror
        #        -Wextra
        #        -Wconversion
        #        -Wpedantic
        #        -Werror=vla
)
target_link_libraries(c_roguelike_framework PRIVATE
        SDL3::SDL3
        glad
        #TODO: investigate
        #m was added for linux to work - we are relying on the math stdl lib
        #somewhere, probably stb?
        #m
)

# DEBUG VS RELEASE *************************************************************
option(LEAK_DETECTION "Activate Leak detection" ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(__DEBUG__)
    if (LEAK_DETECTION)
        add_compile_definitions("__LEAK_DETECTION__")
    endif ()
else ()
    target_link_libraries(c_roguelike_framework PRIVATE -static roguelike_game)
endif ()

# EMSCRIPTEN *******************************************************************
if (EMSCRIPTEN)
    set_target_properties(c_roguelike_framework PROPERTIES
            SUFFIX ".html"
            OUTPUT_NAME "index"
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
endif ()


# COMPILER SPECIFIC*************************************************************

# MINGW ************************************************************************
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message("MinGW")
    target_link_libraries(c_roguelike_framework PRIVATE -static -pthread)
endif ()

# MSVC *************************************************************************
# we use MSVC CRT only to detect memory leaks
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    message("MSVC")
    if (LEAK_DETECTION)
        add_compile_definitions("__MSVC_CRT_LEAK_DETECTION__")
    endif ()
endif ()