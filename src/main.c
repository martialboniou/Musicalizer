#include <assert.h>
#include <complex.h>
#include <math.h>
#include <raylib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARRAY_LEN(xs) sizeof(xs) / sizeof(xs[0])
#define N 256

float pi;

float in[N];
float complex out[N];
float max_amp;

typedef struct {
    float left;
    float right;
} Frame;

void fft(float in[], size_t stride, float complex out[], size_t n) {

    assert(n > 0);
    if (n == 1) {
        out[0] = in[0] + I*in[0];
        return;
    }

    // symmetry so half is calculated
    fft(in, stride * 2, out, n / 2);
    fft(in + stride, stride * 2, out + n / 2, n / 2);

    for (size_t k = 0; k < n / 2; ++k) {
        float t = (float)k / n; // 0 <= t <= 1
        float complex v = cexp(-2 * I * pi * t) * out[k + n / 2];
        float complex e = out[k];
        out[k] = e + v;
        out[k + n / 2] = e - v;
    }
}

float amp(float complex z) {
    float a = fabsf(crealf(z));
    float b = fabsf(cimagf(z));
    if (a < b)
        return b;
    return a;
}

void callback(void *bufferData, unsigned int frames) {

    if (frames < N)
        return;

    Frame *fs = bufferData;

    for (size_t i = 0; i < frames; ++i) {
        in[i] = fs[i].left;
    }

    fft(in, 1, out, N);

    max_amp = 0.0f;
    for (size_t i = 0; i < frames; ++i) {
        float a = amp(out[i]);
        if (max_amp < a)
            max_amp = a;
    }
}

int main(void) {
    pi = atan2f(1, 1) * 4; // approx. used in FORTRAN; atanf(1) = π/4

    InitWindow(800, 600, "Musicalizer");
    SetTargetFPS(60);

    InitAudioDevice();
    Music music = LoadMusicStream("Kalaido - Hanging Lanterns.ogg");
    assert(music.stream.sampleSize == 16);
    assert(music.stream.channels == 2);

    PlayMusicStream(music);
    SetMusicVolume(music, 0.5f);
    AttachAudioStreamProcessor(music.stream, callback);

    while (!WindowShouldClose()) {
        UpdateMusicStream(music);
        if (IsKeyPressed(KEY_SPACE)) {
            if (IsMusicStreamPlaying(music)) {
                PauseMusicStream(music);
            } else {
                ResumeMusicStream(music);
            }
        }

        int w = GetRenderWidth();
        int h = GetRenderHeight();

        BeginDrawing();
        ClearBackground(CLITERAL(Color){0x18, 0x18, 0x18, 0xFF});
        float cell_width = (float)w / N;
        for (size_t i = 0; i < N; ++i) {
            float t = amp(out[i]) / max_amp;
            DrawRectangle(i * cell_width, h / 2 - h / 2 * t, 1, h / 2 * t, RED);
        }
        EndDrawing();
    }

    return 0;
}
