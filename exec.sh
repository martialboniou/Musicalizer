RAYLIB_PATH="${HOME}/.local"

DYLD_FORCE_FLAT_NAMESPACE=1 \
    # DYLD_INSERT_LIBRARIES=./build/libplug.dylib\
    DYLD_LIBRARY_PATH=./build:$RAYLIB_PATH/lib: \
    ./build/musicalizer ./music/Kalaido\ -\ Hanging\ Lanterns.ogg
