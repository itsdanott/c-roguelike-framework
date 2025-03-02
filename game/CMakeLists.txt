if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_library(roguelike_game SHARED
        game.h
        game.c
    )
    set_target_properties(roguelike_game PROPERTIES
            OUTPUT_NAME "game"
            PREFIX ""
    )
else()
    add_library(roguelike_game STATIC
        game.h
        game.c
    )
endif()