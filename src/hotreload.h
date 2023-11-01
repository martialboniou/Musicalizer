#ifndef HOTRELOAD_H_
#define HOTRELOAD_H_

#include <stdbool.h>

#include "plug.h"

#ifdef HOTRELOAD
// eg: `extern plug_init_t *plug_init;`
bool reload_libplug();
#define PLUG(name, ...) extern name##_t *name;
#else
#define reload_libplug() true
#define PLUG(name, ...) name##_t name;
#endif // HOTRELOAD
LIST_OF_PLUGS
#undef PLUG

#endif // HOTRELOAD_H_
