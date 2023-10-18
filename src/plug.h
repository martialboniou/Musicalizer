#ifndef PLUG_H_
#define PLUG_H_

#include "raylib.h"
#include <complex.h>
#include <stddef.h>

#define LIST_OF_PLUGS                                                          \
    PLUG(plug_init, void, void)                                                \
    PLUG(plug_pre_reload, void *, void)                                        \
    PLUG(plug_post_reload, void, void *)                                       \
    PLUG(plug_update, void, void)

#define PLUG(name, ret, ...) typedef ret(name##_t)(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG
// eg: `typedef void (plug_init_t)(const char *file_path)`
// NOTE: no * (as in (*plug_init_t)) b/c we'll choose to use it directly
//       or by pointer from the caller; the latter case for hot reloading

#endif // PLUG_H_
