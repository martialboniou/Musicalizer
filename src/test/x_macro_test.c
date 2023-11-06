// test these macros with the command `cpp`
#define LIST_OF_PLUGS                                                          \
    PLUG(plug_hello)                                                           \
    PLUG(plug_init)                                                            \
    PLUG(plug_pre_reload)                                                      \
    PLUG(plug_post_reload)                                                     \
    PLUG(plug_update)

#define PLUG(name) name##_t name = NULL;
LIST_OF_PLUGS
#undef PLUG

#define PLUG(name)                                                             \
    name = dlsym(libplug, #name);                                              \
    if (name == NULL) {                                                        \
        fprintf(stderr, "ERROR: could not find %s symbol in %s: %s", #name,    \
                libplug_file_name, dlerror());                                 \
        return false;                                                          \
    }
LIST_OF_PLUGS
#undef PLUG
