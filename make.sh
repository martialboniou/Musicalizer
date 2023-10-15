#!/usr/bin/env bash
set -xe

CFLAGS="-Wall -Wextra `pkg-config --cflags raylib`"
LIBS="`pkg-config --libs raylib` -L/opt/homebrew/lib -lm -ldl -lpthread"

clang $CFLAGS -o ./build/libplug.dylib -fPIC -shared ./src/plug.c $LIBS
clang $CFLAGS -o ./build/musicalizer ./src/main.c $LIBS
