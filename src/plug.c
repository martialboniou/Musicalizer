#include "plug.h"
#include "ffmpeg.h"
#include "raylib.h"
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _WINDOWS_
#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif // __APPLE__
#include "miniaudio.h"
#define NOB_IMPLEMENTATION
#include "nob.h"

#define GLSL_VERSION  330

#define N             (1 << 13)
#define FONT_SIZE     64

#define RENDER_FPS    30
#define RENDER_FACTOR 100
#define RENDER_WIDTH  (16 * RENDER_FACTOR)
#define RENDER_HEIGHT (9 * RENDER_FACTOR)

// Microsoft could not update their parser OMEGALUL:
// https://learn.microsoft.com/en-us/cpp/c-runtime-library/complex-math-support?view=msvc-170#types-used-in-complex-math
#ifdef _MSC_VER
#define Float_Complex _Fcomplex
#define cfromreal(re) _FCbuild(re, 0)
#define cfromimag(im) _FCbuild(0, im)
#define mulcc         _FCmulcc
#define addcc(a, b)   _FCbuild(crealf(a) + crealf(b), cimagf(a) + cimagf(b))
#define subcc(a, b)   _FCbuild(crealf(a) - crealf(b), cimagf(a) - cimagf(b))
#else
#define Float_Complex float complex
#define cfromreal(re) (re)
#define cfromimag(im) ((im)
#define mulcc(a, b) ((a) * (b))
#define addcc(a, b) ((a) + (b))
#define subcc(a, b) ((a) - (b))
#endif

typedef struct {
    char *file_path;
    Music music;
} Sample;

typedef struct {
    Sample *items;
    size_t count;
    size_t capacity;
} Samples;

typedef struct {
    // visualizer
    Samples samples;
    int current_sample;
    Font font;
    Shader circle;
    int circle_radius_location;
    int circle_power_location;

    // renderer
    bool rendering;
    RenderTexture2D screen;
    Wave wave;
    float *wave_samples;
    size_t wave_cursor;
    FFMPEG *ffmpeg;

    // FFT analyzer
    float in_raw[N];
    float in_win[N];
    Float_Complex out_raw[N];
    float out_log[N];
    float out_smooth[N];
    float out_smear[N];

    // microphone
    ma_device *microphone;
    bool capturing;
} Plug;

Plug *p = NULL;

bool fft_settled()
{
    float eps = 1e-3;
    for (size_t i = 0; i < N; ++i) {
        if (p->out_smooth[i] > eps)
            return false;
        if (p->out_smear[i] > eps)
            return false;
    }
    return true;
}

void fft_clean()
{
    memset(p->in_raw, 0, sizeof(p->in_raw));
    memset(p->in_win, 0, sizeof(p->in_win));
    memset(p->out_raw, 0, sizeof(p->out_raw));
    memset(p->out_log, 0, sizeof(p->out_log));
    memset(p->out_smooth, 0, sizeof(p->out_smooth));
    memset(p->out_smear, 0, sizeof(p->out_smear));
}

void fft(float in[], size_t stride, Float_Complex out[], size_t n)
{

    assert(n > 0);

    if (n == 1) {
        out[0] = cfromreal(in[0]);
        return;
    }

    // symmetry so half is calculated
    fft(in, stride * 2, out, n / 2);
    fft(in + stride, stride * 2, out + n / 2, n / 2);

    for (size_t k = 0; k < n / 2; ++k) {
        float t = (float)k / n; // normalized
        Float_Complex v = cexp(-2 * I * PI * t) * out[k + n / 2];
        Float_Complex e = out[k];
        out[k] = addcc(e, v);
        out[k + n / 2] = subcc(e, v);
    }
}

static inline float amp(Float_Complex z)
{
    float a = crealf(z);
    float b = cimagf(z);
    return logf(a * a + b * b);
}

size_t fft_analyze(float dt)
{

    // Hann function to smoothen the input (it enhances the output)
    for (size_t i = 0; i < N; ++i) {
        float t = (float)i / (N - 1);
        float hann = 0.5 - 0.5 * cosf(2 * PI * t);
        p->in_win[i] = p->in_raw[i] * hann;
    }

    // FFT
    fft(p->in_win, 1, p->out_raw, N);

    // squash into the logarithmic scale
    float step = 1.06f;
    float lowf = 1.0f;
    size_t m = 0;
    float max_amp = 1.0f;
    for (float f = lowf; (size_t)f < N / 2; f = ceilf(f * step)) {
        float f1 = ceilf(f * step);
        float a = 0.0f;
        for (size_t q = (size_t)f; q < N / 2 && q < (size_t)f1; ++q) {
            float b = amp(p->out_raw[q]);
            if (b > a)
                a = b;
        }
        if (max_amp < a)
            max_amp = a;
        p->out_log[m++] = a;
    }

    // normalize frequencies to 0..1 range
    for (size_t i = 0; i < m; ++i) {
        p->out_log[i] /= max_amp;
    }

    // smooth out and smear the values
    float smoothness = 8;
    float smearness = 3;
    for (size_t i = 0; i < m; ++i) {
        p->out_smooth[i] +=
            (p->out_log[i] - p->out_smooth[i]) * smoothness * dt;
        p->out_smear[i] +=
            (p->out_smooth[i] - p->out_smear[i]) * smearness * dt;
    }

    return m;
}

void fft_push(float frame)
{
    memmove(p->in_raw, p->in_raw + 1, (N - 1) * sizeof(p->in_raw[0]));
    p->in_raw[N - 1] = frame;
}

void callback(void *bufferData, unsigned int frames)
{

    // https://cdecl.org/?q=float+%28*fs%29%5B2%5D (ptr of array 2 of float)
    float(*fs)[2] = bufferData;

    for (size_t i = 0; i < frames; ++i) {
        fft_push(fs[i][0]); // left output only (for now!)
    }
}

void ma_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                 ma_uint32 frameCount)
{
    callback((void *)pInput, frameCount);
    (void)pOutput;
    (void)pDevice;
}

void plug_init()
{
    p = malloc(sizeof(*p));
    assert(p != NULL && "Upgrade your memory!!");
    memset(p, 0, sizeof(*p)); // fill a block of memory

    p->font = LoadFontEx("./resources/fonts/Alegreya-Regular.ttf", FONT_SIZE,
                         NULL, 0);

    p->circle = LoadShader(
        NULL, TextFormat("./resources/shaders/glsl%d/circle.fs", GLSL_VERSION));
    p->circle_radius_location = GetShaderLocation(p->circle, "radius");
    p->circle_power_location = GetShaderLocation(p->circle, "power");

    p->screen = LoadRenderTexture(RENDER_WIDTH, RENDER_HEIGHT);
    p->current_sample = -1;
}

/** TODO: returns Plug* as last track: unused for now */
Plug *plug_pre_reload(void)
{
    for (size_t i = 0; i < p->samples.count; ++i) {
        Sample *it = &p->samples.items[i];
        DetachAudioStreamProcessor(it->music.stream, callback);
    }
    return p;
}

void plug_post_reload(Plug *prev)
{
    p = prev;
    for (size_t i = 0; i < p->samples.count; ++i) {
        Sample *it = &p->samples.items[i];
        AttachAudioStreamProcessor(it->music.stream, callback);
    }
    UnloadShader(p->circle);
    p->circle = LoadShader(
        NULL, TextFormat("./resources/shaders/glsl%d/circle.fs", GLSL_VERSION));
    p->circle_radius_location = GetShaderLocation(p->circle, "radius");
    p->circle_power_location = GetShaderLocation(p->circle, "power");
}

Sample *current_sample()
{
    if (0 <= p->current_sample &&
        (size_t)p->current_sample < p->samples.count) {
        return &p->samples.items[p->current_sample];
    }
    return NULL;
}

void fft_render(size_t w, size_t h, size_t m)
{

    // width of a single bar
    float cell_width = (float)w / m;

    // global color parameters
    float saturation = 0.75f;
    float value = 1.0f;

    // display the bars
    for (size_t i = 0; i < m; ++i) {
        float t = p->out_smooth[i];
        float hue = (float)i / m;
        Color color = ColorFromHSV(hue * 360, saturation, value);
        Vector2 startPos = {
            i * cell_width + cell_width / 2,
            h - (float)h * 2 / 3 * t,
        };
        Vector2 endPos = {
            i * cell_width + cell_width / 2,
            h,
        };
        float thick = cell_width / 3 * sqrtf(t);
        DrawLineEx(startPos, endPos, thick, color);
    }

    Texture2D texture = {rlGetTextureIdDefault(), 1, 1, 1,
                         PIXELFORMAT_UNCOMPRESSED_R8G8B8};

    // display the smears
    SetShaderValue(p->circle, p->circle_radius_location, (float[1]){0.3f},
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p->circle, p->circle_power_location, (float[1]){3.0f},
                   SHADER_UNIFORM_FLOAT);
    BeginShaderMode(p->circle);
    for (size_t i = 0; i < m; ++i) {
        float start = p->out_smear[i];
        float end = p->out_smooth[i];
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
        float radius = cell_width * 3 * sqrtf(end);
        Vector2 origin = {0};
        if (endPos.y >= startPos.y) {
            // up
            Rectangle dest = {.x = startPos.x - radius / 2,
                              .y = startPos.y,
                              .width = radius,
                              .height = endPos.y - startPos.y};
            Rectangle source = {0, 0, 1, 0.5};
            DrawTexturePro(texture, source, dest, origin, 0, color);
        } else {
            // down
            Rectangle dest = {.x = endPos.x - radius / 2,
                              .y = endPos.y,
                              .width = radius,
                              .height = startPos.y - endPos.y};
            Rectangle source = {0, 0.5, 1, 0.5};
            DrawTexturePro(texture, source, dest, origin, 0, color);
        }
    }
    EndShaderMode();

    // display the circles
    SetShaderValue(p->circle, p->circle_radius_location, (float[1]){0.07f},
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p->circle, p->circle_power_location, (float[1]){5.0f},
                   SHADER_UNIFORM_FLOAT);
    BeginShaderMode(p->circle);
    for (size_t i = 0; i < m; ++i) {
        float t = p->out_smooth[i];
        float hue = (float)i / m;
        Color color = ColorFromHSV(hue * 360, saturation, value);
        Vector2 center = {
            i * cell_width + cell_width / 2,
            h - (float)h * 2 / 3 * t,
        };
        float radius = cell_width * 6 * sqrtf(t);
        Vector2 position = {
            .x = center.x - radius,
            .y = center.y - radius,
        };
        DrawTextureEx(texture, position, 0, 2 * radius, color);
    }
    EndShaderMode();
}

void error_load_file_popup()
{
    // TODO: implement annoying popup that indicates we could not load file
    TraceLog(LOG_ERROR, "Could not load file");
}

void plug_update()
{

    // if Apple Retina, ensure FLAG_WINDOW_HIGHDPI is set before InitWindow()
    // FIXME: 2023-11-14 still an issue with raylib 5.0 dev
#ifdef __APPLE__
    int w = 960;
    int h = 540;
#else
    int w = GetRenderWidth();
    int h = GetRenderHeight();
#endif

    BeginDrawing();
    ClearBackground(GetColor(0x151515FF));

    if (!p->rendering) { // preview mode
        if (p->capturing) {
            if (p->microphone != NULL) {
                if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_M)) {
                    ma_device_uninit(p->microphone);
                    p->microphone = NULL;
                    p->capturing = false;
                }

                size_t m = fft_analyze(GetFrameTime());
                fft_render(w, h, m);
            } else {
                if (IsKeyPressed(KEY_ESCAPE)) {
                    p->capturing = false;
                }

                const char *label = "Capture Device Error: Check the Logs";
                Color color = RED;
                int fontSize = p->font.baseSize;
                Vector2 size = MeasureTextEx(p->font, label, fontSize, 0);
                Vector2 position = {
                    (float)w / 2 - size.x / 2,
                    (float)h / 2 - size.y / 2,
                };
                DrawTextEx(p->font, label, position, fontSize, 0, color);

                label = "(Press ESC to continue)";
                fontSize = p->font.baseSize * 2 / 3;
                size = MeasureTextEx(p->font, label, fontSize, 0);
                position.x = (float)w / 2 - size.x / 2;
                position.x = (float)w / 2 - size.x / 2;
                DrawTextEx(p->font, label, position, fontSize, 0, color);
            }

        } else {
            if (IsFileDropped()) {
                FilePathList droppedFiles = LoadDroppedFiles();
                for (size_t i = 0; i < droppedFiles.count; ++i) {
                    char *file_path = strdup(droppedFiles.paths[i]);

                    Sample *sample = current_sample();
                    if (sample) {
                        StopMusicStream(sample->music);
                    }

                    // WIP
                    Music music = LoadMusicStream(file_path);

                    if (IsMusicReady(music)) {
                        SetMusicVolume(music, 0.5f);
                        AttachAudioStreamProcessor(music.stream, callback);
                        PlayMusicStream(music);

                        nob_da_append(&p->samples, (CLITERAL(Sample){
                                                       .file_path = file_path,
                                                       .music = music,
                                                   }));
                        p->current_sample = p->samples.count - 1;
                    } else {
                        free(file_path);
                        error_load_file_popup();
                    }
                }
                UnloadDroppedFiles(droppedFiles);
            }

            if (IsKeyPressed(KEY_M)) {
                // TODO: let the user choose their mic

                ma_device_config deviceConfig =
                    ma_device_config_init(ma_device_type_capture);
                deviceConfig.capture.format = ma_format_f32;
                deviceConfig.capture.channels = 2;
                deviceConfig.sampleRate = 44100;
                deviceConfig.dataCallback = ma_callback;
                deviceConfig.pUserData = NULL;

                p->microphone = malloc(sizeof(ma_device));
                assert(p->microphone != NULL && "Buy more RAM!!");
                ma_result result =
                    ma_device_init(NULL, &deviceConfig, p->microphone);
                if (result != MA_SUCCESS) {
                    TraceLog(
                        LOG_ERROR,
                        "MINIAUDIO: Failed to initialize capture device: %s",
                        ma_result_description(result));
                }

                if (p->microphone != NULL) {
                    ma_result result = ma_device_start(p->microphone);
                    if (result != MA_SUCCESS) {
                        TraceLog(LOG_ERROR,
                                 "MINIAUDIO: Failed to start device: %s",
                                 ma_result_description(result));
                        ma_device_uninit(p->microphone);
                        p->microphone = NULL;
                    }
                }
                p->capturing = true;
            }

            Sample *sample = current_sample();
            if (sample) { // music is loaded and ready
                UpdateMusicStream(sample->music);

                if (IsKeyPressed(KEY_SPACE)) {
                    if (IsMusicStreamPlaying(sample->music)) {
                        PauseMusicStream(sample->music);
                    } else {
                        ResumeMusicStream(sample->music);
                    }
                }

                if (IsKeyPressed(KEY_Q)) {
                    StopMusicStream(sample->music);
                    PlayMusicStream(sample->music);
                }

                if (IsKeyPressed(KEY_F)) {
                    StopMusicStream(sample->music);

                    fft_clean();
                    // TODO: LoadWave is pretty slow on big files
                    p->wave = LoadWave(sample->file_path);
                    p->wave_cursor = 0;
                    p->wave_samples = LoadWaveSamples(p->wave);
                    p->ffmpeg = ffmpeg_start_rendering(
                        p->screen.texture.width, p->screen.texture.height,
                        RENDER_FPS, sample->file_path);
                    p->rendering = true;
                    SetTraceLogLevel(LOG_WARNING);
                }

                size_t m = fft_analyze(GetFrameTime());
                fft_render(w, h, m);

            } else { // waiting for the user to DnD some tracks...

                const char *label = "Drag&Drop Music Here";
                Color color = WHITE;
                Vector2 size =
                    MeasureTextEx(p->font, label, p->font.baseSize, 0);
                Vector2 position = {
                    (float)w / 2 - size.x / 2,
                    (float)h / 2 - size.y / 2,
                };
                DrawTextEx(p->font, label, position, p->font.baseSize, 0,
                           color);
            }

            DrawTextEx(p->font,
                       TextFormat("Samples loaded: %zu", p->samples.count),
                       CLITERAL(Vector2){0}, 64, 0, WHITE);
        }
    } else { // rendering mode
        Sample *sample = current_sample();
        NOB_ASSERT(sample != NULL);
        if (p->ffmpeg == NULL) { // starting FFMPEG process has failed
            if (IsKeyPressed(KEY_ESCAPE)) {
                SetTraceLogLevel(LOG_INFO);
                UnloadWave(p->wave);
                UnloadWaveSamples(p->wave_samples);
                p->rendering = false;
                fft_clean();
                PlayMusicStream(sample->music);
            }

            const char *label = "FFmpeg Failure: Check the Logs";
            Color color = RED;
            int fontSize = p->font.baseSize;
            Vector2 size = MeasureTextEx(p->font, label, fontSize, 0);
            Vector2 position = {
                (float)w / 2 - size.x / 2,
                (float)h / 2 - size.y / 2,
            };
            DrawTextEx(p->font, label, position, fontSize, 0, color);

            label = "(Press ESC to Continue)";
            fontSize = p->font.baseSize * 2 / 3;
            size = MeasureTextEx(p->font, label, fontSize, 0);
            position.x = (float)w / 2 - size.x / 2;
            position.y = (float)h / 2 - size.y / 2 + fontSize;
            DrawTextEx(p->font, label, position, fontSize, 0, color);
        } else { // FFMPEG process is going
            // TODO: introduce a rendering mode that perfectly loops the video
            if ((p->wave_cursor >= p->wave.frameCount && fft_settled()) ||
                IsKeyPressed(KEY_ESCAPE)) {
                if (!ffmpeg_end_rendering(p->ffmpeg)) {
                    p->ffmpeg = NULL;
                } else {
                    SetTraceLogLevel(LOG_INFO);
                    UnloadWave(p->wave);
                    UnloadWaveSamples(p->wave_samples);
                    p->rendering = false;
                    fft_clean();
                    PlayMusicStream(sample->music);
                }
            } else { // rendering...

                // label
                const char *label = "Rendering video...";
                Color color = WHITE;

                Vector2 size =
                    MeasureTextEx(p->font, label, p->font.baseSize, 0);
                Vector2 position = {
                    (float)w / 2 - size.x / 2,
                    (float)h / 2 - size.y / 2,
                };
                DrawTextEx(p->font, label, position, p->font.baseSize, 0,
                           color);

                // progress bar
                float bar_width = (float)w * 2 / 3;
                float bar_height = p->font.baseSize * 0.25;
                float bar_progress = (float)p->wave_cursor / p->wave.frameCount;
                float bar_padding_top = p->font.baseSize * 0.5;
                if (bar_progress > 1)
                    bar_progress = 1;
                float bar_x = (float)w / 2 - bar_width / 2;
                float bar_y = (float)p->font.baseSize / 2 + bar_padding_top;

                Rectangle bar_filling = {
                    .x = bar_x,
                    .y = bar_y,
                    .width = bar_width * bar_progress,
                    .height = bar_height,
                };
                DrawRectangleRec(bar_filling, WHITE);

                Rectangle bar_box = {
                    .x = bar_x,
                    .y = bar_y,
                    .width = bar_width,
                    .height = bar_height,
                };
                DrawRectangleLinesEx(bar_box, 2, WHITE);

                // rendering
                size_t chunk_size = p->wave.sampleRate / RENDER_FPS;
                // https://cdecl.org/?q=float+%28*fs%29%5B2%5D
                float *fs = (float *)p->wave_samples;
                for (size_t i = 0; i < chunk_size; ++i) {
                    if (p->wave_cursor < p->wave.frameCount) {
                        fft_push(fs[p->wave_cursor * p->wave.channels + 0]);
                    } else {
                        fft_push(0);
                    }
                    p->wave_cursor += 1;
                }

                size_t m = fft_analyze(1.0f / RENDER_FPS);

                BeginTextureMode(p->screen);
                ClearBackground(GetColor(0x151515FF));
                fft_render(p->screen.texture.width, p->screen.texture.height,
                           m);
                EndTextureMode();

                Image image = LoadImageFromTexture(p->screen.texture);
                if (!ffmpeg_send_frame_flipped(p->ffmpeg, image.data,
                                               p->screen.texture.width,
                                               p->screen.texture.height)) {
                    ffmpeg_end_rendering(p->ffmpeg);
                    p->ffmpeg = NULL;
                }
                UnloadImage(image);
            }
        }
    }

    EndDrawing();
}
