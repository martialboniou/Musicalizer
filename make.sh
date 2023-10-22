#!/usr/bin/env bash
set -xe

# TEMPORARY: use git current sources (10/2023) instead of brew's version
RAYLIB_PATH="${HOME}/.local"

# CFLAGS="-Wall -Wextra `pkg-config --cflags raylib`"
# LIBS="`pkg-config --libs raylib` -L/opt/homebrew/lib -ldl -lpthread"
CFLAGS="-Wall -Wextra -I${RAYLIB_PATH}/include"
LIBS="-L${RAYLIB_PATH}/lib -lraylib -L/opt/homebrew/lib -ldl -lpthread"

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


#read -p "Do you want to debug this program? "
#if [[ $REPLY =~ ^[Yy]$ ]]; then
#    DBG_OPTIONS="-g"
#fi

clang $DBG_OPTIONS $CFLAGS ${SOFLAGS} -fPIC -o ./build/libplug.${SO} ./src/plug.c $LIBS
# clang $DBG_OPTIONS $CFLAGS -o ./build/musicalizer ./src/plug.c ./src/main.c $LIBS
clang $DBG_OPTIONS $CFLAGS -DHOTRELOAD -o ./build/musicalizer ./src/main.c $LIBS

# TODO: zsh version
#if read -q "choice?Do you want to debug this program? "; then
#    DBG_OPTIONS="-g"
#fi
