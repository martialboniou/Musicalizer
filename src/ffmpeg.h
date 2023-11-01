#ifndef FFMPEG_H_
#define FFMPEG_H_

#include <stdbool.h>
#include <stddef.h>

typedef void FFMPEG;

FFMPEG *ffmpeg_start_rendering(size_t width, size_t height, size_t fps,
                               const char *audio_file_path);
bool ffmpeg_send_frame_flipped(FFMPEG *ffmpeg, void *data, size_t width,
                               size_t height);
bool ffmpeg_end_rendering(FFMPEG *ffmpeg);

#endif // FFMPEG_H_
