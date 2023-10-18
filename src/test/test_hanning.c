#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

/* test the Hann function to smoothen the input before a Fourier transform */
// clang -o test_hanning test_hanning.c && ./test_hanning

#define N 30

float in[N];
float complex out[N];

void dft(float in[], float complex out[], size_t n) {
    for (size_t f = 0; f < n; ++f) {
        out[f] = 0;
        for (size_t i = 0; i < n; ++i) {
            float t = (float)i / n; // normalized
            out[f] += in[i] * cexp(2 * I * M_PI * f * t);
        }
    }
}

int main() {

    /* original Hann function */
    // for (size_t i = 0; i < N; ++i) {
    //     float t = (float)i / (N - 1);
    //     float hann = 0.5 - 0.5 * cosf(2 * M_PI * t);
    //     for (size_t j = 0; j < hann * N; ++j) {
    //         printf("*");
    //     }
    //     printf("\n");
    // }

    float f = 3.12f; // ie between 1st & 2nd harmonics
    float offset = 0.5;
    for (size_t i = 0; i < N; ++i) {
        float t = (float)i / N; // normalized (N - 1) from w in Hann ~= N
        float hann = 0.5 - 0.5 * cosf(2 * M_PI * t);
        in[i] = sinf(2 * M_PI * f * t + offset) * hann; // smoothened input
    }

    dft(in, out, N);

    float max = 0;
    for (size_t i = 0; i < N; ++i) {
        float a = cabsf(out[i]);
        if (max < a)
            max = a;
    }

    for (size_t i = 0; i < N / 2; ++i) {
        float a = cabsf(out[i]);
        float t = a / max; // normalized
        for (size_t j = 0; j < t * N; ++j) {
            printf("*");
        }
        printf("\n");
    }

    for (size_t i = 0; i < N; ++i)
        printf("-");
    printf("\n");

    for (size_t k = 0; k < 2; ++k) { // expected break in periodicity
        for (size_t i = 0; i < N; ++i) {
            float t = (in[i] + 1) / 2;
            for (size_t j = 0; j < t * N; ++j) {
                printf(" ");
            }
            printf("*\n");
        }
    }

    return 0;
}
