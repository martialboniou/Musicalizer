#include "plug.h"
#include "raylib.h"
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N (1 << 13)
#define FONT_SIZE 69

typedef struct {
    Music music;
    Font font;
    bool error;
} Plug;

Plug *plug = NULL;

float in1[N];
float in2[N];
float complex out[N];

void fft(float in[], size_t stride, float complex out[], size_t n) {

    assert(n > 0);

    if (n == 1) {
        out[0] = in[0];
        return;
    }

    // symmetry so half is calculated
    fft(in, stride * 2, out, n / 2);
    fft(in + stride, stride * 2, out + n / 2, n / 2);

    for (size_t k = 0; k < n / 2; ++k) {
        float t = (float)k / n; // normalized
        float complex v = cexp(-2 * I * PI * t) * out[k + n / 2];
        float complex e = out[k];
        out[k] = e + v;
        out[k + n / 2] = e - v;
    }
}

float amp(float complex z) {
    // float a = fabsf(crealf(z));
    // float b = fabsf(cimagf(z));
    // if (a < b)
    //     return b;
    // return a;
    float a = crealf(z);
    float b = cimagf(z);
    return logf(a * a + b * b);
}

void callback(void *bufferData, unsigned int frames) {

    // https://cdecl.org/?q=float+%28*fs%29%5B2%5D (ptr of array 2 of float)
    float(*fs)[2] = bufferData;

    for (size_t i = 0; i < frames; ++i) {
        memmove(in1, in1 + 1, (N - 1) * sizeof(in1[0]));
        in1[N - 1] = fs[i][0]; // left output only (for now!)
    }
}

void plug_init() {
    plug = malloc(sizeof(*plug));
    assert(plug != NULL && "Upgrade your memory!!");
    memset(plug, 0, sizeof(*plug)); // fill a block of memory

    plug->font = LoadFontEx("./resources/fonts/Alegreya-Regular.ttf", FONT_SIZE,
                            NULL, 0);

    plug->error = false;

    // plug->music = LoadMusicStream(file_path);
    // assert(plug->music.stream.channels == 2);

    // SetMusicVolume(plug->music, 0.5f);
    // AttachAudioStreamProcessor(plug->music.stream, callback);
    // PlayMusicStream(plug->music);
}

/** TODO: returns Plug* as last track: unused for now */
Plug *plug_pre_reload() {
    if (IsMusicReady(plug->music)) {
        DetachAudioStreamProcessor(plug->music.stream, callback);
    }
    return plug;
}

void plug_post_reload(void *prev) {
    plug = prev;
    if (IsMusicReady(plug->music)) {
        AttachAudioStreamProcessor(plug->music.stream, callback);
    }
}

void plug_update() {
    if (IsMusicReady(plug->music)) {
        UpdateMusicStream(plug->music);
    }
    if (IsKeyPressed(KEY_SPACE)) {
        if (IsMusicReady(plug->music)) {
            if (IsMusicStreamPlaying(plug->music)) {
                PauseMusicStream(plug->music);
            } else {
                ResumeMusicStream(plug->music);
            }
        }
    }

    if (IsKeyPressed(KEY_Q)) {
        if (IsMusicReady(plug->music)) {
            StopMusicStream(plug->music);
            PlayMusicStream(plug->music);
        }
    }

    if (IsFileDropped()) {
        FilePathList droppedFiles = LoadDroppedFiles();
        // for (size_t i = 0; i < droppedFiles.count; ++i) {
        if (droppedFiles.count > 0) {
            const char *file_path = droppedFiles.paths[0];

            if (IsMusicReady(plug->music)) {
                DetachAudioStreamProcessor(plug->music.stream, callback);
                StopMusicStream(plug->music);
                UnloadMusicStream(plug->music);
            }

            plug->music = LoadMusicStream(file_path);

            if (IsMusicReady(plug->music)) {
                plug->error = false;
                printf("music.frameCount = %u\n", plug->music.frameCount);
                printf("music.stream.sampleRate = %u\n",
                       plug->music.stream.sampleRate);
                printf("music.stream.sampleSize = %u\n",
                       plug->music.stream.sampleSize);
                printf("music.stream.channels = %u\n",
                       plug->music.stream.channels);
                SetMusicVolume(plug->music, 0.5f);
                AttachAudioStreamProcessor(plug->music.stream, callback);
                PlayMusicStream(plug->music);
            } else {
                plug->error = true;
            }
        }
        UnloadDroppedFiles(droppedFiles);
    }

    int w = GetRenderWidth();
    int h = GetRenderHeight();

    BeginDrawing();
    ClearBackground(CLITERAL(Color){0x18, 0x18, 0x18, 0xFF});

    if (IsMusicReady(plug->music)) {
        // Hann function to smoothen the input (it enhances the output)
        for (size_t i = 0; i < N; ++i) {
            float t = (float)i / N; // N ~= N - 1 so close enough to the formula
            float hann = 0.5 - 0.5 * cosf(2 * PI * t);
            in2[i] = in1[i] * hann;
        }

        fft(in2, 1, out, N);

        float max_amp = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            float a = amp(out[i]);
            if (max_amp < a)
                max_amp = a;
        }

        float step = 1.06;
        float lowf = 1.0f;
        size_t m = 0;
        // start at 20Hz
        for (float f = lowf; (size_t)f < N / 2; f = ceilf(f * step)) {
            m += 1;
        }

        float cell_width = (float)w / m;
        m = 0;
        for (float f = lowf; (size_t)f < N / 2; f = ceilf(f * step)) {
            float f1 = ceilf(f * step);
            float a = 0.0f;
            for (size_t q = (size_t)f; q < N / 2 && q < (size_t)f1; ++q) {
                float b = amp(out[q]);
                if (b > a)
                    a = b;
            }
            float t = a / max_amp;
            Color c = ColorAlphaBlend(RED, ColorAlpha(GREEN, t), WHITE);
            DrawRectangle(m * cell_width, h - (float)h / 2 * t, cell_width,
                          (float)h / 2 * t, c);
            m += 1;
        }
    } else {
        const char *label;
        Color color;
        if (plug->error) {
            label = "Could not load file";
            color = RED;
        } else {
            label = "Drag&Drop Music Here";
            color = WHITE;
        }
        Vector2 size = MeasureTextEx(plug->font, label, plug->font.baseSize, 0);
        Vector2 position = {
            (float)w / 2 - size.x / 2,
            (float)h / 2 - size.y / 2,
        };
        DrawTextEx(plug->font, label, position, plug->font.baseSize, 0, color);
    }
    EndDrawing();
}
