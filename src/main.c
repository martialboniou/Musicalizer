#include "plug.h"
#include <assert.h>
#include <raylib.h>
#include <stdio.h>

#include <dlfcn.h>

// char *shift_args(int *argc, char ***argv) {
//     assert(*argc > 0);
//     char *result = (**argv);
//     (*argv) += 1;
//     (*argc) -= 1;
//     return result;
// }

const char *libplug_file_name = "libplug.dylib";
void *libplug = NULL;

#ifdef HOTRELOAD
#define PLUG(name, ...) name##_t *name = NULL;
// eg: `plug_init_t *plug_init = NULL;`
#else
#define PLUG(name, ...) name##_t name;
#endif
LIST_OF_PLUGS
#undef PLUG
// - LIST_OF_PLUGS
// - #undef PLUG

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

#define PLUG(name, ...)                                                             \
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

int main() {

    if (!reload_libplug())
        return 1;

    // const char *program = shift_args(&argc, &argv);

    // if (argc == 0) {
    //     fprintf(stderr, "Usage: %s <input>\n", program);
    //     fprintf(stderr, "ERROR: no input file is provided\n");
    //     return 1;
    // }
    // const char *file_path = shift_args(&argc, &argv);

    size_t factor = 60;
    InitWindow(factor*16, factor*9, "Musicalizer");
    SetTargetFPS(60);
    InitAudioDevice();

    plug_init(); // used the file_path as arg previously

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_R)) {
            void *state = plug_pre_reload();
            if (!reload_libplug())
                return 1;
            plug_post_reload(state);
        }
        plug_update();
    }

    return 0;
}
