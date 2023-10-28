#!/usr/bin/env bash
# test with wine64
# TODO: find ../../.. -name ffmpeg\*\.exe
x86_64-w64-mingw32-gcc -o hello.exe ./hello_mingw_from_mac.c && wine64 ./hello.exe
PROCESS="ffmpeg.exe"
CWD_REL_LOCATION=../../..
if [[ ! -f "${PROCESS}" ]]; then
    find $CWD_REL_LOCATION -type f -name "${PROCESS}" \
        ! -path "${CWD_REL_LOCATION}/src/test/windows/*" -print0 | \
            xargs -0 -I {} cp -a {} "${PROCESS}"
fi
x86_64-w64-mingw32-gcc -o create_child_proc.exe ./create_child_proc.c && wine64 ./create_child_proc.exe
