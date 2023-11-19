Based on this [series](https://www.youtube.com/playlist?list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT
)
of video by Tsoding.

Intro
=====

This version is aimed to work on macOS (for now; tested on 13+ and 14.1+).
The macOS dynamic library version works (don't touch the file named VERSION,
it is required).
The shared library version is untested (I'll check on Linux later; I recommend
to check the original version [here](https://github.com/tsoding/musializer)).

![Example of display](./resources/images/musicalizer_by_tsoding_macos_version.png "Example of display (WIP)")

About dylib
===========

For now, the **hot reloading** works while compiling the dynamic library
properly (no need to `codesign` by the way). Here's the command to configure
and compile (notice that the first line is required the very first time
only; `-r` & `-m` for the configuration stands for **r**eloading &
**m**icrophone capturing respectively; `./nob` starts with the `build` option
by default):

```sh
cc -o nob nob.c
./nob config -r -m && ./nob
```

For example:
- change the `exec.sh` path to the audio file (`flac` is only available if
  you build `raylib` with the flag `SUPPORT_FILEFORMAT_FLAC`)
- run the program with `exec.sh` (and let it run!)
- edit `src/plug.c` (say, change the color of a text element)
- change the last number (AKA patch) in the file `VERSION` (otherwise, the
  OS will use the same *cached* `libplug.dylib`
- run `./nob config -r -m && ./nob` (to recompile the dynamic library; even
  if in this case, it'll recompile and link the program too; you also can run
  `make.sh` instead; if `nob` doesn't exist at the root of this project,
  just execute: `cc -o nob nob.c`)
- switch back on the Musicalizer window
- type the `h` key as set on a QWERTY layout (`d` on Dvorak)
  it will load the newest version of the plugin (and thus, change the color if
  you did so in `src/plug.c`)

About GLES (not required)
=========================

As OpenGL is deprecated, and Metal is way faster, you might have to compile
`raylib` (with the complete source code) for the web platform GL ES in order
to enable the **shaders** on macOS;
the linkage will be made in the `./make.sh`; you just need a Google Chrome
installed or the libaries `libEGL.dylib` and `libGLESv2.dylib` manually
installed in the `./build` directory: check
[here](https://github.com/grplyler/raylib-articles#3-quickstart-short-version-using-angle-from-your-browser).

So, **first**, ensure `raylib` has been compiled with `GRAPHICS_API_OPENGL_ES`:

```sh
# use me in raylib/src

make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=SHARED GRAPHICS=GRAPHICS_API_OPENGL_ES && \
    mkdir -p $HOME/.local/{include,lib};\
    cp -a raylib.h raymath.h rlgl.h $HOME/.local/include &&\
    cp -a libraylib* $HOME/.local/lib
```

Beware, you must use: `#version 100` for your `./shaders/circle.fs` in this
configuration (not by default; see the next paragraph),
instead of the one mentioned in the
[tutorial](https://www.youtube.com/watch?v=1pqIg-Ug7bU&list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT&index=7).
Check some [examples](https://github.com/raysan5/raylib/blob/master/examples/shaders/resources/shaders/glsl100/bloom.fs)
of `glsl100` syntax in the raylib source code.

For now (late 10/2023), `raylib` builds the library with the default
graphics and works perfectly with Metal. You don't need this so the
**version 330** of the shader will work as expected.

About audio
===========

I had some issues with early Sonoma & raylib (worked well since the 14.1 upgrade
& raylib 4.6-*devel* sources (@`f721429`); 10-27-2023). Check the CoreAudio tag
appears when you launch `./exec.sh`:

```
INFO: AUDIO: Device initialized successfully
INFO:     > Backend:       miniaudio / Core Audio
```

Short note about `ffmpeg`
=========================

First part
----------

The first part of this note can be skipped: It concerns the code written by Tsoding
during a live recording. The type of `ffmpeg` were `int` instead
of a *struct* also keeping the `pid` of the child process.

In the [source code](https://github.com/tsoding/rendering-video-in-c-with-ffmpeg/tree/1347d5356987f1d9b131a6c59ab72748599dee7f)
of the `rendering-video-in-c-with-ffmpeg` by Tsoding, the `raylib` version
of the code needs this change (it'll be done in the next video; motivation:
the implementation of the *pipe* on windows requires another kind of data
to store)::

```patch
@@ -10,7 +10,7 @@

 int main(void)
 {
-    int ffmpeg = ffmpeg_start_rendering(WIDTH, HEIGHT, FPS);
+    FFMPEG *ffmpeg = ffmpeg_start_rendering(WIDTH, HEIGHT, FPS);

     InitWindow(WIDTH, HEIGHT, "FFmpeg");
     SetTargetFPS(FPS);
```

Second part
-----------

*REMINDER*: beware of the `ffmpeg` order of options (where `-i` is
mandatory for the input and `-` is required, just after this, for a
POSIX *pipe* when sending a data stream);
it's *chunk*-based command so:

```sh
ffmpeg [global_options]
{[input_file_options] -i input_url} ...
{[input_file_options] -i input_url} ...
{[input_file_options] -i input_url} ...
{[output_file_options] -i output_url} ...
```

The code
--------

In this archive, `ffmpeg.h` and its implementations come from [here](https://github.com/tsoding/musializer/blob/master/src/ffmpeg.h); 
here is the [license](https://github.com/tsoding/musializer/blob/master/LICENSE).

About Windows port
==================

This can be done from macOS or linux (I'd try this from FreeBSD at some point).
You'll need two compiled libraries. You'll need to install them manually 
(it's not that painful but you can skip it; if you're
bored, also skip it; windows almost exists; just to boot Excel or Chrome;
poorly maintained for video games (have you ever played a retro 95-99 games on 
modern computers? Direct3D? Windows Live?); Android clients
and Linux are the norm; Linux all the way! BSD is the best, btw: but Linux
rules)

FFMPEG
------

About the Windows port ([video 9](https://www.youtube.com/watch?v=EB96Auoag6g&list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT&index=9)),
you need to install `ffmpeg.exe` (check [this version](https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-2023-10-18-git-e7a6bba51a-essentials_build.7z)).

The current implementation works with `ffmpeg.exe` in `./build/ffmpeg/bin/`.

RayLib (obsolete)
-----------------

RayLib is included in the code and can be compiled with windows. Before that,
you had to download [this archive](https://github.com/raysan5/raylib/releases/download/4.6-dev/raylib-4.6-dev_win64_mingw-w64.zip)
and unzip to `./build/raylib-windows`.

Cross-compilation
-----------------

Run the `make.sh` script like this:

```sh
DISABLE_WINDOWS_COMPILE=false ./make.sh
```

As expected, doing so will create  a `PE-32` executable for windows.

Run & Thoughts
--------------

If you don't have a windows os, use `wine64` to run `./build/musicalizer.exe`.
Who needs windows? On my Apple Silicon hardware, GLFW crashes:
I decided to postpone this issue (TODO: an Angle wrapper or something?).

About miniaudio.h
=================

Disable the runtime linking on macOS and manually link the frameworks at
the compilation. Here's what must be added to use `miniaudio.h` properly
(it might be fixed later; 11/2023):

```c
#define MINIAUDIO_IMPLEMENTATION
#ifdef __APPLE__
    #define MA_NO_RUNTIME_LINKING
#endif
```

For example, try:

```sh
cd ./src/test
clang -o test_audio_play test_audio_play.c -lpthread -ldl -lm -framework CoreFoundation -framework CoreAudio -framework AudioToolbox
./test_audio_play
```

It should play the `sound.wav` file (`ffmpeg -i ../../music/<your file>.ogg sound.wav`)..
To test the recording functionality, compile and run with these commands (in the
same directory):

```sh
clang -o test_audio_rec test_audio_rec.c -lpthread -ldl -lm -framework CoreFoundation -framework CoreAudio -framework AudioToolbox
./test_audio_rec
```

`nob` builder AKA No Build Urself™
==================================

This part was added during the Music Visualizer part 11, 12 and 13.
Compile `nob` first:

```sh
cc -o nob nob.c
```

You won't need to compile this file again. Run `./nob` with some options
on your system (here, macOS):

```sh
./nob config -r -m
./nob
./exec.sh
```

You can build for windows (64-bit, btw!):

```sh
./nob config -t win64-mingw
./nob
wine64 ./build/musicalizer.exe
```

Build `raylib` from `nob`
=========================

This part was written during the [13th video](https://www.youtube.com/watch?v=wH963jJ1lRM&list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT&index=14).
The `raylib` source code must be put in the `./raylib` directory.
Note that the `-ObjC` flag must be added to compile `rglfw` on macOS.

```sh
git clone https://github.com/raysan5/raylib
./nob config && ./nob
./nob config -r -m && ./nob
```
