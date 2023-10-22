#include "plug.h"
#include "raylib.h"
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rlgl.h>

#define N (1 << 13)
#define FONT_SIZE 69

typedef struct {
    Music music;
    Font font;
    Shader circle;
    Shader smear;
    bool error;
} Plug;

Plug *plug = NULL;

float in_raw[N];
float in_win[N];
float complex out_raw[N];
float out_log[N];
float out_smooth[N];
float out_smear[N];

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
    float a = crealf(z);
    float b = cimagf(z);
    return logf(a * a + b * b);
}

void callback(void *bufferData, unsigned int frames) {

    // https://cdecl.org/?q=float+%28*fs%29%5B2%5D (ptr of array 2 of float)
    float(*fs)[2] = bufferData;

    for (size_t i = 0; i < frames; ++i) {
        memmove(in_raw, in_raw + 1, (N - 1) * sizeof(in_raw[0]));
        in_raw[N - 1] = fs[i][0]; // left output only (for now!)
    }
}

void plug_init() {
    plug = malloc(sizeof(*plug));
    assert(plug != NULL && "Upgrade your memory!!");
    memset(plug, 0, sizeof(*plug)); // fill a block of memory

    plug->font = LoadFontEx("./resources/fonts/Alegreya-Regular.ttf", FONT_SIZE,
                            NULL, 0);

    plug->circle = LoadShader(NULL, "./shaders/circle.fs");
    plug->smear = LoadShader(NULL, "./shaders/smear.fs");

    plug->error = false;
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
    UnloadShader(plug->circle);
    plug->circle = LoadShader(NULL, "./shaders/circle.fs");
    UnloadShader(plug->smear);
    plug->smear = LoadShader(NULL, "./shaders/smear.fs");
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
    float dt = GetFrameTime();

    BeginDrawing();
    ClearBackground(CLITERAL(Color){0x15, 0x15, 0x15, 0xFF});

    if (IsMusicReady(plug->music)) {
        // Hann function to smoothen the input (it enhances the output)
        for (size_t i = 0; i < N; ++i) {
            float t = (float)i / (N - 1);
            float hann = 0.5 - 0.5 * cosf(2 * PI * t);
            in_win[i] = in_raw[i] * hann;
        }

        fft(in_win, 1, out_raw, N);

        float step = 1.06f;
        float lowf = 1.0f;
        size_t m = 0;
        float max_amp = 1.0f;
        for (float f = lowf; (size_t)f < N / 2; f = ceilf(f * step)) {
            float f1 = ceilf(f * step);
            float a = 0.0f;
            for (size_t q = (size_t)f; q < N / 2 && q < (size_t)f1; ++q) {
                float b = amp(out_raw[q]);
                if (b > a)
                    a = b;
            }
            if (max_amp < a)
                max_amp = a;
            out_log[m++] = a;
        }

        // normalize frequencies to 0..1 range
        for (size_t i = 0; i < m; ++i) {
            out_log[i] /= max_amp;
        }

        float smoothness = 8;
        float smearness = 5;
        for (size_t i = 0; i < m; ++i) {
            out_smooth[i] += (out_log[i] - out_smooth[i]) * smoothness * dt;
            out_smear[i] += (out_smooth[i] - out_smear[i]) * smearness * dt;
        }

        float cell_width = (float)w / m;
        float saturation = 0.75f;
        float value = 1.0f;

        // display the bars
        for (size_t i = 0; i < m; ++i) {
            float t = out_smooth[i];
            float hue = (float)i / m;
            float saturation = 0.75f;
            float value = 1.0f;
            Color color = ColorFromHSV(hue * 360, saturation, value);
            Vector2 startPos = {
                i * cell_width + cell_width / 2,
                h - (float)h * 2 / 3 * t,
            };
            Vector2 endPos = {
                i * cell_width + cell_width / 2,
                h,
            };
            float thick = cell_width / 2 * sqrt(t);
            DrawLineEx(startPos, endPos, thick, color);
        }

        Texture2D texture = {rlGetTextureIdDefault(), 1, 1, 1,
                             PIXELFORMAT_UNCOMPRESSED_R8G8B8};

        // display the smears
        BeginShaderMode(plug->smear);
        for (size_t i = 0; i < m; ++i) {
            float start = out_smear[i];
            float end = out_smooth[i];
            float hue = (float)i / m;
            Color color = ColorFromHSV(hue * 360, saturation, value);
            Vector2 startPos = {
                i * cell_width + cell_width / 2,
                h - (float)h * 2 / 3 * start,
            };
            Vector2 endPos = {
                i * cell_width + cell_width / 2,
                h - (float)h * 2 / 3 * end,
            };
            float radius = cell_width * sqrtf(end);
            Vector2 origin = {0};
            if (endPos.y >= startPos.y) {
                // up
                Rectangle dest = {.x = startPos.x - radius,
                                  .y = startPos.y,
                                  .width = 2 * radius,
                                  .height = endPos.y - startPos.y};
                Rectangle source = {0, 0, 1, 0.5};
                DrawTexturePro(texture, source, dest, origin, 0, color);
            } else {
                // down
                Rectangle dest = {.x = endPos.x - radius,
                                  .y = endPos.y,
                                  .width = 2 * radius,
                                  .height = startPos.y - endPos.y};
                Rectangle source = {0, 0.5, 1, 0.5};
                DrawTexturePro(texture, source, dest, origin, 0, color);
            }
        }
        EndShaderMode();

        // display the circles
        BeginShaderMode(plug->circle);
        for (size_t i = 0; i < m; ++i) {
            float t = out_smooth[i];
            float hue = (float)i / m;
            Color color = ColorFromHSV(hue * 360, saturation, value);
            Vector2 center = {
                i * cell_width + cell_width / 2,
                h - (float)h * 2 / 3 * t,
            };
            float radius = cell_width * 5 * sqrtf(t); // smaller, was 8
            Vector2 position = {
                .x = center.x - radius,
                .y = center.y - radius,
            };
            DrawTextureEx(texture, position, 0, 2 * radius, color);
        }
        EndShaderMode();

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
