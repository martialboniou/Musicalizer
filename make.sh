#!/usr/bin/env bash
cc -o nob nob.c && echo "nob has been compiled."
echo "The rest of this file is for testing only."
echo "Run \`./nob config -r && ./nob build\`"
exit 0;

set -xe

: "${DISABLE_WINDOWS_COMPILE:=true}"
: "${DISABLE_DEBUGGING_NATIVELY:=true}"

# TEMPORARY: use git current sources (10/2023) instead of brew's version
RAYLIB_PATH="${HOME}/.local"
RAYLIB_PATH_INCLUDE="${RAYLIB_PATH}/include"
RAYLIB_PATH_LIB="${RAYLIB_PATH}/lib"
# CFLAGS="-Wall -Wextra `pkg-config --cflags raylib`"
# LIBS="`pkg-config --libs raylib` -L/opt/homebrew/lib -ldl -lpthread"
CFLAGS="-Wall -Wextra -I${RAYLIB_PATH_INCLUDE}"
LIBS="-L${RAYLIB_PATH_LIB} -lraylib -L/opt/homebrew/lib -ldl -lpthread"

VER=`(cat VERSION)`
MAJOR=`(cut -d . -f 1 VERSION)`
MINOR=`(cut -d . -f 2 VERSION)`

if [ "$(uname -s)" = Darwin ]; then
    SO=dylib
    SOFLAGS="-dynamiclib -install_name libplug.${MAJOR}.dylib -current_version ${VER} -compatibility_version ${MAJOR}.${MINOR}.0"
    # append the frameworks as miniaudio.h does MA_NO_RUNTIME_LINKING
    LIBS+=" -framework CoreFoundation -framework CoreAudio -framework AudioToolbox"
    # -- ANGLE (Google Chrome required)
    # if [[ ! -x "./build/libEGL.dylib" || ! -x "./build/libGLESv2.dylib" ]]; then

    #     ANGLE_EGL_LIB=`find /Applications/Google\ Chrome.app -name 'libEGL.dylib' | tail -1`
    #     ANGLE_GLES_LIB=`find /Applications/Google\ Chrome.app -name 'libGLESv2.dylib' | tail -1`

    #     if [ "${ANGLE_EGL_LIB}" = "" || "${ANGLE_GLES_LIB}" = ""]; then
    #         exit 1 # won't be able to link Angle to use the GL Shaders (mandatory on aarch)
    #     else
    #         cp "${ANGLE_EGL_LIB}" ./build/libEGL.dylib
    #         cp "${ANGLE_GLES_LIB}" ./build/libGLESv2.dylib
    #     fi
    # fi
    # temporary remove libGLESv2 => compile raylib sources
    # ANGLE_LIBS="./build/libEGL.dylib ./build/libGLESv2.dylib"
    # LIBS+=$ANGLE_LIBS
else
    SO=so
    SOFLAGS="-shared -Wl,-soname,libderp.so.$MAJOR.$MINOR"

fi

if [ $DISABLE_DEBUGGING_NATIVELY != true ]; then
    read -p "Do you want to debug this program? "
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        DBG_OPTIONS="-g"
    fi
fi

clang $DBG_OPTIONS $CFLAGS ${SOFLAGS} -fPIC -o ./build/libplug.${SO} \
    ./src/plug.c ./src/ffmpeg.c \
    ./src/separate_translation_unit_for_miniaudio.c \
    $LIBS
clang $DBG_OPTIONS $CFLAGS -DHOTRELOAD -o ./build/musicalizer \
    ./src/main.c ./src/hotreload.c \
    ./src/separate_translation_unit_for_miniaudio.c \
    $LIBS

### the next one is to compile the static library version on macOS
# clang $DBG_OPTIONS $CFLAGS -o ./build/musicalizer \
#     ./src/ffmpeg.c ./src/plug.c \
#     ./src/separate_translation_unit_for_miniaudio.c ./src/main.c \
#     ./build/raylib/posix/libraylib.a \
#     -framework "OpenGL" -framework "Cocoa" \
#     -framework "IOKit" -framework "CoreAudio" -framework "CoreVideo"

### the next one is to compile for windows (WIP)
if [ $DISABLE_WINDOWS_COMPILE != true ]; then
    read -p "Do you want to compile a binary for windows too? "
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        x86_64-w64-mingw32-gcc \
            -Wall -Wextra -DWINDOWS \
            -I./build/raylib-windows/include \
            -o ./build/musicalizer.exe \
            ./src/ffmpeg_windows.c \
            ./src/plug.c ./src/main.c \
            -L./build/raylib-windows/lib \
            -lraylib -lwinmm -lgdi32 \
            -static
    fi
fi

# NOTE: copy ffmpeg.exe at the root of the project or ensure it's in
#       your wine64 path
