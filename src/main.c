#include "plug.h"
#include <assert.h>
#include <raylib.h>
#include <stdio.h>

#include <dlfcn.h>

char *shift_args(int *argc, char ***argv) {
    assert(*argc > 0);
    char *result = (**argv);
    (*argv) += 1;
    (*argc) -= 1;
    return result;
}

const char *libplug_file_name = "libplug.dylib";
void *libplug = NULL;

#ifdef HOTRELOAD
#define PLUG(name) name##_t *name = NULL;
// eg: `plug_init_t *plug_init = NULL;` from PLUG(plug_init) in LIST_OF_PLUGS
//     w/ `typedef void (plug_init_t)(Plug *p, const char *fp)` (BEWARE: no *)
#else
#define PLUG(name) name##_t name;
#endif
LIST_OF_PLUGS
#undef PLUG
// - LIST_OF_PLUGS
// - #undef PLUG

Plug plug = {0};

#ifdef HOTRELOAD
bool reload_libplug(void) {
    // if (libplug != NULL)
    //     dlclose(libplug);

    libplug = dlopen(libplug_file_name, RTLD_NOW);
    if (libplug == NULL) {
        fprintf(stderr, "ERROR: could not load %s: %s", libplug_file_name,
                dlerror());
        return false;
    }

#define PLUG(name)                                                             \
    name = dlsym(libplug, #name);                                              \
    if (name == NULL) {                                                        \
        fprintf(stderr, "ERROR: could not find %s symbol in %s: %s", #name,    \
                libplug_file_name, dlerror());                                 \
        return false;                                                          \
    }
    LIST_OF_PLUGS
#undef PLUG

    return true;
}
#else
#define reload_libplug() true
#endif

int main(int argc, char **argv) {

    if (!reload_libplug())
        return 1;

    const char *program = shift_args(&argc, &argv);

    // TODO: supply input files via drag&drop
    if (argc == 0) {
        fprintf(stderr, "Usage: %s <input>\n", program);
        fprintf(stderr, "ERROR: no input file is provided\n");
        return 1;
    }
    const char *file_path = shift_args(&argc, &argv);

    InitWindow(800, 600, "Musicalizer");
    SetTargetFPS(60);
    InitAudioDevice();

    plug_init(&plug, file_path);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_R)) {
            plug_pre_reload(&plug);
            if (!reload_libplug())
                return 1;
            plug_post_reload(&plug);
        }
        plug_update(&plug);
    }

    return 0;
}
