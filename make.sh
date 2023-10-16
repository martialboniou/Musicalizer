#!/usr/bin/env bash
set -xe

CFLAGS="-Wall -Wextra `pkg-config --cflags raylib`"
LIBS="`pkg-config --libs raylib` -L/opt/homebrew/lib -lm -ldl -lpthread"

VER=`(cat VERSION)`
MAJOR=`(cut -d . -f 1 VERSION)`
MINOR=`(cut -d . -f 2 VERSION)`

if [ "$(uname -s)" = Darwin ]; then
    SO=dylib
    SOFLAGS="-dynamiclib -install_name libplug.${MAJOR}.dylib -current_version ${VER} -compatibility_version ${MAJOR}.${MINOR}.0"
else
    SO=so
    SOFLAGS="-shared -Wl,-soname,libderp.so.$MAJOR.$MINOR"

fi
clang $CFLAGS ${SOFLAGS} -fPIC -o ./build/libplug.${SO} ./src/plug.c $LIBS
clang $CFLAGS -o ./build/musicalizer ./src/main.c $LIBS
