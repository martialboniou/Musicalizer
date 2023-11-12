#/usr/bin/env bash
DYLD_FORCE_FLAT_NAMESPACE=1 \
    DYLD_LIBRARY_PATH=./build:./build/raylib/posix: ./build/musicalizer
