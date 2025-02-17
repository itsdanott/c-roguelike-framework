# Building
## Web (Emscripten)
On Windows run:
```shell
mkdir cmake-emscripten
cd cmake-emscripten
emcmake.bat cmake -G Ninja ..
ninja.exe
```
This expects ninja and emscripten are correctly set up and added to PATH. 