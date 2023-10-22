Based on this [series](https://www.youtube.com/playlist?list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT
)
of video by Tsoding.

This version is aimed to work on macOS (for now; tested on 13+). The shared
library version is untested (I'll check on Linux later; I recommend to check
the original version [here](https://github.com/tsoding/musializer)).

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

As OpenGL is deprecated, and Metal is way faster, you will have to compile
`raylib` for the web platform GL ES in order to enable the **shaders** on macOS;
the linkage will be made in the `./make.sh`; you just need a Google Chrome
installed or the libaries `libEGL.dylib` and `libGLESv2.dylib` manually
installed in the `./build` directory: check
[here](https://github.com/grplyler/raylib-articles#3-quickstart-short-version-using-angle-from-your-browser)
So, **first**, ensure `raylib` has been compiled with `GRAPHICS_API_OPENGL_ES`:

```sh
# use me in raylib/src

make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=SHARED GRAPHICS=GRAPHICS_API_OPENGL_ES && \
    mkdir -p $HOME/.local/{include,lib};\
    cp -a raylib.h raymath.h rlgl.h $HOME/.local/include &&\
    cp -a libraylib* $HOME/.local/lib
```

Beware, you must use: `#version 100` for your `./shaders/circle.fs` now,
instead of the one mentioned in the
[tutorial](https://www.youtube.com/watch?v=1pqIg-Ug7bU&list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT&index=7).
Check some [examples](https://github.com/raysan5/raylib/blob/master/examples/shaders/resources/shaders/glsl100/bloom.fs)
of `glsl100` syntax in the raylib source code.

Don't upgrade to Sonoma yet; raylib audio output doesn't work (tested with
homebrew & sources (10/2023))
