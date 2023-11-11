#!/usr/bin/env bash
clang -o core_basic_windows -I../../raylib/src ../../build/raylib/posix/libraylib.a ./core_basic_window.c -framework OpenGL -framework Cocoa -framework IOKit
