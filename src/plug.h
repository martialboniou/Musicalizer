#ifndef PLUG_H_
#define PLUG_H_

#include "raylib.h"
#include <complex.h>
#include <stddef.h>

#define N 256

typedef struct {
    Music music;
} Plug;

typedef void (*plug_hello_t)(void);
typedef void (*plug_init_t)(Plug *plug, const char *file_path);
typedef void (*plug_update_t)(Plug *plug);

#endif // PLUG_H_
