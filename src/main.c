#include <assert.h>
#include <raylib.h>
#include <stdio.h>

#include "plug.h"

#ifndef _WIN32
#include <signal.h> // needed for sigaction()
#endif              // _WIN32

#include "hotreload.h"
#include "separate_translation_unit_for_miniaudio.h"

int main()
{
#ifndef _WIN32
    // NOTE: This is needed because if the pipe between this program and FFmpeg
    // breaks. The program will receive SIGPIPE on trying to write into it.
    // While such behavior makes sense for command line utilities, this program
    // is a relatively friendly GUI application that is trying to recover from
    // such situations.
    struct sigaction act = {0};
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
#endif // _WIN32

    if (!reload_libplug())
        return 1;

    size_t factor = 60;
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(factor * 16, factor * 9, "Musicalizer");
    SetTargetFPS(60);
    InitAudioDevice();

    plug_init(); // used the file_path as arg previously

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_H)) {
            void *state = plug_pre_reload();
            if (!reload_libplug())
                return 1;
            plug_post_reload(state);
        }
        plug_update();
    }

    return 0;
}
