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
plug_hello_t plug_hello = NULL;
plug_init_t plug_init = NULL;
plug_update_t plug_update = NULL;
Plug plug = {0};

bool reload_libplug(void) {
    //if (libplug != NULL)
    //    dlclose(libplug);

    libplug = dlopen(libplug_file_name, RTLD_NOW);
    if (libplug == NULL) {
        fprintf(stderr, "ERROR: could not load %s: %s", libplug_file_name,
                dlerror());
        return false;
    }

    plug_hello = dlsym(libplug, "plug_hello");
    if (plug_hello == NULL) {
        fprintf(stderr, "ERROR: could not find plug_hello symbol in %s: %s",
                libplug_file_name, dlerror());
        return false;
    }

    // return true; // test plug_hello only

    plug_init = dlsym(libplug, "plug_init");
    if (plug_init == NULL) {
        fprintf(stderr, "ERROR: could not find plug_init symbol in %s: %s",
                libplug_file_name, dlerror());
        return false;
    }

    plug_update = dlsym(libplug, "plug_update");
    if (plug_update == NULL) {
        fprintf(stderr, "ERROR: could not find plug_update symbol in %s: %s",
                libplug_file_name, dlerror());
        return false;
    }

    return true;
}

int main(int argc, char **argv) {

    if (!reload_libplug())
        return 1;

    // libplug = dlopen("libotherplug.dylib", RTLD_LOCAL | RTLD_NOW);
    // plug_hello = dlsym(libplug, "plug_hello");
    // plug_init = dlsym(libplug, "plug_init");
    // plug_update = dlsym(libplug, "plug_update");
    // plug_hello();

    /* // test plug_hello only
    libplug = dlopen("libotherplug.dylib", RTLD_NOW);
    plug_hello = dlsym(libplug, "plug_hello");

    plug_hello();

    if (!reload_libplug())
        return 1;

    plug_hello();

    return 0;
    */

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
            if (!reload_libplug())
                return 1;
            plug_hello(); // TODO: remove me
        }
        plug_update(&plug);
    }

    return 0;
}
