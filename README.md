
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
- replace stb function defs with SDL ones to avoid C runtime library
- UI Layout persistence accross aspect ratios and resolutions
- Textbox alignment(pivots)
- Add miniaudio
- Add FNL
- Tilemap rendering
- Game simulation

## Emscripten
Run ```build-emscripten``` from the project root to build for web. 

In order for the shell script to run properly make sure to add to path:
- [Ninja](https://ninja-build.org/)
- [emsdk](https://github.com/emscripten-core/emsdk)

### Itch.io
If you want to auto deploy to itch.io also add:
- [butler](https://github.com/itchio/butler)

to your PATH.

After running ```butler login``` *once* you can auto deploy to itch.io simply by 
appending this line to your build-emscripten script:
```shell
butler push .\bin-Emscripten\ user-name/project-name:channel-name
```
Or add a new ```build-deploy-itch``` script:

Batch example:
```batch
call build-emscripten.bat
cd ..
call butler push .\bin-Emscripten\ user-name/project-name:channel-name
```
For more details see the [butler docs gitbook](https://itch.io/docs/butler/pushing.html) 

