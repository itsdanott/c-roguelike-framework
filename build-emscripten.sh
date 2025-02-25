#!/bin/sh

mkdir -p cmake-emscripten
cd cmake-emscripten || exit
emcmake cmake -G Ninja ..
ninja