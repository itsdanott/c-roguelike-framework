if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(__DEBUG__)
    add_library(roguelike_game SHARED
            game.h
            game.c
            ../third-party/FastNoiseLite/C/FastNoiseLite.h
    )
    set_target_properties(roguelike_game PROPERTIES
            OUTPUT_NAME "game"
            PREFIX ""
    )
else ()
    add_compile_definitions(__GAMELIB_STATIC_LINK__)
    add_library(roguelike_game STATIC
            game.h
            game.c
            ../third-party/FastNoiseLite/C/FastNoiseLite.h
    )
endif ()

target_include_directories(roguelike_game PRIVATE "../third-party/SDL/include")

target_link_libraries(roguelike_game PRIVATE
        SDL3::SDL3
)