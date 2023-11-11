#include <stdio.h>

// Instructions
// 1. put a sound.wav file at the root of src/test
// ffmpeg -i ../../music/<your preferred audio file>.ogg sound.wav
// 2. compile (here, instruction for macOS Sonoma; the frameworks are required)
// clang -o test_audio_rec test_audio_rec.c -lpthread -ldl -lm -framework
// CoreFoundation -framework CoreAudio -framework AudioToolbox

#define MINIAUDIO_IMPLEMENTATION
#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif
#define MA_DEBUG_OUTPUT
#include "../miniaudio.h"

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
    ma_encoder *pEncoder = (ma_encoder *)pDevice->pUserData;
    MA_ASSERT(pEncoder != NULL);

    ma_encoder_write_pcm_frames(pEncoder, pInput, frameCount, NULL);

    (void)pOutput;
}

int main() {

    ma_encoder encoder;
    ma_encoder_config encoderConfig =
        ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);
    const char *output_file_path = "output.wav";
    if (ma_encoder_init_file(output_file_path, &encoderConfig, &encoder) !=
        MA_SUCCESS) {
        fprintf(stderr, "ERROR: could not initialize %s\n", output_file_path);
        return -1;
    }

    ma_device_config deviceConfig =
        ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = encoder.config.format;
    deviceConfig.capture.channels = encoder.config.channels;
    deviceConfig.sampleRate = encoder.config.sampleRate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &encoder;

    ma_device device;
    ma_result result;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();

    ma_device_uninit(&device);
    ma_encoder_uninit(&encoder);

    return 0;
}
