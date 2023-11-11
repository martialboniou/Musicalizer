#include <stdio.h>

// Instructions
// 1. put a sound.wav file at the root of src/test
// ffmpeg -i ../../music/<your preferred audio file>.ogg sound.wav
// 2. compile (here, instruction for macOS Sonoma; the frameworks are required)
// clang -o test_audio_play test_audio_play.c -lpthread -ldl -lm -framework
// CoreFoundation -framework CoreAudio -framework AudioToolbox

#define MINIAUDIO_IMPLEMENTATION
#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif
#define MA_DEBUG_OUTPUT
#include "../miniaudio.h"

int main()
{

    ma_engine engine;
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS)
        return -1;

    const char *sound_file_path = "sound.wav";
    result = ma_engine_play_sound(&engine, sound_file_path, NULL);

    if (result != MA_SUCCESS) {
        fprintf(stderr, "ERROR: could not load \"%s\"\n", sound_file_path);
        return -1;
    }

    printf("Press Enter to quit...\n");
    getchar();

    ma_engine_uninit(&engine);

    return 0;
}
