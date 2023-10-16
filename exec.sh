DYLD_FORCE_FLAT_NAMESPACE=1 \
    # DYLD_INSERT_LIBRARIES=./build/libplug.dylib\
    DYLD_LIBRARY_PATH="./build" \
    ./build/musicalizer music/Kalaido\ -\ Hanging\ Lanterns.ogg
