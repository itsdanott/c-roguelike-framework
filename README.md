
# C Roguelike Framework
This is a lightweight C framework in preparation for 7drl 2025.
It also serves as a learning project to get started with SDL3.

For simplicity, we use GL 3.3 core for all desktop platforms and GL ES 3.0
for web as the feature set of these two gl versions is quite close to each
other.

## Target platforms
- Windows
- Linux
- macOS
- Emscripten

## Third Party Libraries
- [SDL3](https://github.com/libsdl-org/SDL)
- [stb](https://github.com/nothings/stb/)
- [miniaudio](https://github.com/mackron/miniaudio)
- [FastNoise Lite](https://github.com/Auburn/FastNoiseLite/)

## TODO
- Arena allocator
- replace sdl function defs with SDL ones to avoid C runtime library