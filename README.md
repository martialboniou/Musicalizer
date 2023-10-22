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

Don't upgrade to Sonoma yet; raylib audio output doesn't work (tested with
sources too (10/2023))
