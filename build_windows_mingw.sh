#!/bin/sh

echo "deprecated file (for test only)"
exit 0

set -xe

mkdir -p ./build/
x86_64-w64-mingw32-windres ./src/musializer.rc -O coff -o ./build/musializer.res
x86_64-w64-mingw32-gcc -mwindows -Wall -Wextra -g \
    -I./build/raylib-windows/include  -o ./build/musializer.exe \
    ./src/plug.c ./src/ffmpeg_windows.c \
    ./src/separate_translation_unit_for_miniaudio.c ./src/musializer.c \
    ./build/musializer.res -L./build/raylib-windows/lib \
    -lraylib -lwinmm -lgdi32 -static
cp -r ./resources ./build
cp -r ./musializer-logged.bat ./build/
