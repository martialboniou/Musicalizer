Based on this [series](https://www.youtube.com/playlist?list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT
)
of video by Tsoding.

Intro
=====

This version is aimed to work on macOS (for now; tested on 13+ and 14.1+).
The shared library version is untested (I'll check on Linux later; I recommend
to check the original version [here](https://github.com/tsoding/musializer)).

![Example of display](./resources/images/musicalizer_by_tsoding_macos_version.png "Example of display (WIP)")

About dylib
===========

For now, the **hot reloading** works while compiling the dynamic library
properly (no need to `codesign` by the way). For example:
- change the `exec.sh` path to the audio file (`flac` unsupported)
- run the program with `exec.sh` (and let it run!)
- edit `src/plug.c` (say, change the color of a `DrawRectangle`) 
- run `make.sh` (to recompile the dynamic library; even if in this case,
  it'll recompile and link the program too)
- switch back on the Musicalizer window
- type the `r` key as set on a QWERTY layout (`p` on Dvorak)
it will load the newest version of the plugin (and thus, change the color if
you did so in `plug.c`)

About GLES (not required)
=========================

As OpenGL is deprecated, and Metal is way faster, you might have to compile
`raylib` for the web platform GL ES in order to enable the **shaders** on macOS;
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

RayLib
------

Download [this archive](https://github.com/raysan5/raylib/releases/download/4.6-dev/raylib-4.6-dev_win64_mingw-w64.zip) first.
Unzip and put the content in `./build/raylib`

Cross-compilation
-----------------

WIP

Run & Thoughts
--------------

If you don't have a windows os, use `wine64` to run `./build/musicalizer.exe`.
Who needs windows? On my Apple Silicon hardware, GLFW crashes.
I'd try later with an Angle wrapper or something.
