#include "separate_translation_unit_for_miniaudio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif // __APPLE__
// #define MA_DEBUG_OUTPUT
#include "miniaudio.h"

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {
    capture_device_callback2_t *capture_device_callback =
        (capture_device_callback2_t *)pDevice->pUserData;
    assert(capture_device_callback != NULL);
    capture_device_callback((void *)pInput, frameCount);
    (void)pOutput;
}

Capture_Device *
init_default_capture_device(capture_device_callback2_t *capture_device_callback) {
    ma_device_config deviceConfig =
        ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = capture_device_callback;

    ma_device *device = malloc(sizeof(ma_device));
    assert(device != NULL && "Buy more RAM!!");
    ma_result result = ma_device_init(NULL, &deviceConfig, device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize capture device.\n");
        return NULL;
    }

    return device;
}

void uninit_capture_device(Capture_Device *device) { ma_device_uninit(device); }

bool start_capture_device(void *device) {
    ma_result result = ma_device_start(device);
    if (result != MA_SUCCESS) {
        printf("Failed to start device.\n");
        return false;
    }

    return true;
}
