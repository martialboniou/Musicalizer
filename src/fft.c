#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

float pi;

void dft(float in[], float complex out[], size_t n) {
    for (size_t f = 1; f < n; ++f) {
        out[f] = 0;
        for (size_t i = 0; i < n; ++i) {
            float t = (float)i / n; // 0 <= t <= 1
            out[f] += in[i] * cexp(2 * I * pi * f * t);
        }
    }
}

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
        float t = (float)k / n; // 0 <= t <= 1
        float complex v = cexp(-2 * I * pi * t) * out[k + n / 2];
        float complex e = out[k];
        out[k] = e + v;
        out[k + n / 2] = e - v;
    }
}

int main() {
    pi = atan2f(1, 1) * 4; // approx. used in FORTRAN; atanf(1) = π/4

    size_t n = 64;        // number of samples
    float in[n];          // input buffer
    float complex out[n]; // output buffer

    for (size_t i = 0; i < n; ++i) {
        float t = (float)i / n; // 0 <= t <= 1
        in[i] = cosf(2 * pi * t) +
                sinf(2 * pi * t * 2); // wave = offset-1Hz + 2Hz harmonics
    }

    // dft(in, out, n);
    fft(in, 1, out, n);

    for (size_t f = 0; f < n; ++f) {
        printf("%02zu: %.2f, %.2f\n", f, creal(out[f]), cimag(out[f]));
    }

    return 0;
}
