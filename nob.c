#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "src/nob.h"

typedef enum {
    TARGET_POSIX,
    // TODO: the target is called Win32 but we are actually building Win64
    TARGET_WIN32_MINGW,
    TARGET_WIN32_MSVC,
    COUNT_TARGETS,
} Target;

const char *target_names[] = {
    [TARGET_POSIX] = "posix",
    [TARGET_WIN32_MINGW] = "win32-mingw",
    [TARGET_WIN32_MSVC] = "win32-msvc",
};
static_assert(3 == COUNT_TARGETS, "Amount of targets have changed");

const char *dbg_option = "-g"; // or "-ggdb; I use lldb"

void log_available_targets(Nob_Log_Level level)
{
    nob_log(level, "Available targets:");
    for (size_t i = 0; i < COUNT_TARGETS; ++i) {
        nob_log(level, "    %s", target_names[i]);
    }
}

typedef struct {
    Target target;
    bool hotreload;
    bool help_requested;
} Config;

typedef struct {
    char major[2];
    char minor[2];
    char patch[3];
} Version;

bool parse_config_from_args(int argc, char **argv, Config *config)
{
    memset(config, 0, sizeof(Config));
#ifdef _WIN32
#if defined(_MSC_VER)
    config->target = TARGET_WIN32_MSVC;
#else
    config->target = TARGET_WIN32_MINGW;
#endif
#else
    config->target = TARGET_POSIX;
#endif // _WIN32

    if (argc > 0) {
        const char *flag = nob_shift_args(&argc, &argv);
        if (strcmp(flag, "-t") == 0) {
            if (argc <= 0) {
                nob_log(NOB_ERROR, "No value is provided for flag &s", flag);
                log_available_targets(NOB_ERROR);
                return false;
            }

            const char *value = nob_shift_args(&argc, &argv);

            bool found = false;
            for (size_t i = 0; !found && i < COUNT_TARGETS; ++i) {
                if (strcmp(target_names[i], value) == 0) {
                    config->target = i;
                    found = true;
                }
            }

            if (!found) {
                nob_log(NOB_ERROR, "Unknown target %s", value);
                log_available_targets(NOB_ERROR);
                return false;
            }
        } else if (strcmp("-r", flag) == 0) {
            config->hotreload = true;
        } else if (strcmp("-h", flag) == 0 || strcmp("--help", flag) == 0) {
            config->help_requested = true;
        } else {
            nob_log(NOB_ERROR, "Unknown flag %s", flag);
            return false;
        }
    }
    return true;
}

void log_config(Config config)
{
    nob_log(NOB_INFO, "Target: %s", NOB_ARRAY_GET(target_names, config.target));
    nob_log(NOB_INFO, "Hotreload: %s",
            config.hotreload ? "ENABLED" : "DISABLED");
}

bool dump_config_to_file(const char *path, Config config)
{
    char line[256];

    Nob_String_Builder sb = {0};

    nob_log(NOB_INFO, "Saving configuration to %s", path);

    snprintf(line, sizeof(line), "target = %s" NOB_LINE_END,
             NOB_ARRAY_GET(target_names, config.target));
    nob_sb_append_cstr(&sb, line);
    snprintf(line, sizeof(line), "hotreload = %s" NOB_LINE_END,
             config.hotreload ? "true" : "false");
    nob_sb_append_cstr(&sb, line);

    if (!nob_write_entire_file(path, sb.items, sb.count))
        return false;

    return true;
}

bool load_version_from_file(const char *path, Version *version)
{
    bool result = true;
    Nob_String_Builder sb = {0};

    nob_log(NOB_INFO, "Loading version from %s", path);

    if (!nob_read_entire_file(path, &sb))
        nob_return_defer(false);

    Nob_String_View content = {
        .data = sb.items,
        .count = sb.count,
    };

    // TODO: manage comment header?
    Nob_String_View line = nob_sv_trim(nob_sv_chop_by_delim(&content, '\n'));
    Nob_String_View major = nob_sv_chop_by_delim(&line, '.');
    Nob_String_View minor = nob_sv_chop_by_delim(&line, '.');

    sprintf(version->major, SV_Fmt, SV_Arg(major));
    sprintf(version->minor, SV_Fmt, SV_Arg(minor));
    sprintf(version->patch, SV_Fmt, SV_Arg(nob_sv_trim(line)));

defer:
    nob_sb_free(sb);
    return result;
}

bool load_config_from_file(const char *path, Config *config)
{
    bool result = true;
    Nob_String_Builder sb = {0};

    nob_log(NOB_INFO, "Loading configuration from  %s", path);

    if (!nob_read_entire_file(path, &sb))
        nob_return_defer(false);

    Nob_String_View content = {
        .data = sb.items,
        .count = sb.count,
    };

    for (size_t row = 0; content.count > 0; ++row) {
        Nob_String_View line =
            nob_sv_trim(nob_sv_chop_by_delim(&content, '\n'));
        if (line.count == 0)
            continue;

        Nob_String_View key = nob_sv_trim(nob_sv_chop_by_delim(&line, '='));
        Nob_String_View value = nob_sv_trim(line);

        if (nob_sv_eq(key, nob_sv_from_cstr("target"))) {
            bool found = false;
            for (size_t t = 0; !found && t < COUNT_TARGETS; ++t) {
                if (nob_sv_eq(value, nob_sv_from_cstr(target_names[t]))) {
                    config->target = t;
                    found = true;
                }
            }
            if (!found) {
                nob_log(NOB_ERROR, "%s:%zu: Invalid target `" SV_Fmt "`", path,
                        row + 1, SV_Arg(value));
                nob_return_defer(false);
            }
        } else if (nob_sv_eq(key, nob_sv_from_cstr("hotreload"))) {
            if (nob_sv_eq(value, nob_sv_from_cstr("true"))) {
                config->hotreload = true;
            } else if (nob_sv_eq(value, nob_sv_from_cstr("false"))) {
                config->hotreload = false;
            } else {
                nob_log(NOB_ERROR, "%s:%zu: Invalid boolean `" SV_Fmt "`", path,
                        row + 1, SV_Arg(value));
                nob_log(NOB_ERROR, "Expected `true` or `false`");

                nob_return_defer(false);
            }
        } else {
            nob_log(NOB_ERROR, "%s:%zu: Invalid key `" SV_Fmt "`", path,
                    row + 1, SV_Arg(key));
            nob_return_defer(false);
        }
    }

defer:
    nob_sb_free(sb);
    return result;
}

void append_frameworks(Nob_Cmd *cmd)
{
    // all because of the way libraylib.a has been built
    nob_cmd_append(cmd, "-framework", "OpenGL");
    nob_cmd_append(cmd, "-framework", "Cocoa");
    nob_cmd_append(cmd, "-framework", "IOKit");
    nob_cmd_append(cmd, "-framework", "CoreAudio");
    nob_cmd_append(cmd, "-framework", "CoreVideo");
    nob_cmd_append(cmd, "-framework", "AudioToolbox");
}

bool build_main(Config config)
{
    bool result = true;
    Nob_Cmd cmd = {0};

    switch (config.target) {
    case TARGET_POSIX: {

        // in case, you need files in ~/.local (here, headers)
        // const char *home = getenv("HOME");
        // nob_temp_sprintf("-I%s/.local/include", home);

        if (config.hotreload) {
            Nob_Procs procs = {0};

            // dynamic library case
            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", dbg_option);
            nob_cmd_append(&cmd, "-I./raylib/src", "-I./src");
#ifdef __APPLE__
            nob_cmd_append(&cmd, "-dynamiclib");
            Version version = {0};
            if (load_version_from_file("./VERSION", &version)) {
                nob_cmd_append(
                    &cmd, "-install_name",
                    nob_temp_sprintf("libplug.%s.dylib", version.major));
                nob_cmd_append(&cmd, "-current_version",
                               nob_temp_sprintf("%s.%s.%s", version.major,
                                                version.minor, version.patch));
                nob_cmd_append(
                    &cmd, "-compatibility_version",
                    nob_temp_sprintf("%s.%s.0", version.major, version.minor));
            }
            nob_cmd_append(&cmd, "-o", "./build/libplug.dylib");
#else
            nob_cmd_append(&cmd, "-fPIC", "-shared", "-o",
                           "./build/libplug.so");
#endif
            nob_cmd_append(&cmd, "./src/plug.c", "./src/ffmpeg.c");
            nob_cmd_append(
                &cmd,
                nob_temp_sprintf("-L./build/raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)),
                "-lraylib");
            nob_cmd_append(&cmd, "-ldl", "-lpthread");
#ifdef __APPLE__
            append_frameworks(&cmd);
#endif
            nob_da_append(&procs, nob_cmd_run_async(cmd));

            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./raylib/src");
            nob_cmd_append(&cmd, "-DHOTRELOAD");
            nob_cmd_append(&cmd, "-o", "./build/musicalizer");
            nob_cmd_append(&cmd, "./src/main.c");
            nob_cmd_append(&cmd, "./src/hotreload.c");

#ifdef __APPLE__
            // TODO: -install_name @rpath/libraylib.dylib ?
            // I use DYLD_LIBRARY_PATH for now
#else
            // NOTE: -rpath= is bad syntax (only works with ld in GNU env)
            const char *rpath = "-Wl,-rpath";
            nob_cmd_append(&cmd, rpath, "./build");
            nob_cmd_append(&cmd, rpath, "./");
            nob_cmd_append(
                &cmd, rpath,
                nob_temp_sprintf("./build/raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)));
            // NOTE: just in case somebody wants to run musicalizer from
            // within the ./build/ folder
            nob_cmd_append(
                &cmd, rpath,
                nob_temp_sprintf("./raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)));
#endif
            nob_cmd_append(
                &cmd,
                nob_temp_sprintf("-L./build/raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)),
                "-lraylib");
            append_frameworks(&cmd);

            nob_da_append(&procs, nob_cmd_run_async(cmd));

            if (!nob_procs_wait(procs))
                nob_return_defer(false);

        } else {
            // static library case
            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", dbg_option);
            nob_cmd_append(&cmd, "-I./raylib/src");
            nob_cmd_append(&cmd, "-o", "./build/musicalizer");
            nob_cmd_append(&cmd, "./src/plug.c", "./src/ffmpeg.c",
                           "./src/main.c");
            // nob_cmd_append(&cmd, "-L./build/raylib", "-lraylib");
            nob_cmd_append(
                &cmd,
                nob_temp_sprintf("./build/raylib/%s/libraylib.a",
                                 NOB_ARRAY_GET(target_names, config.target)));
            nob_cmd_append(&cmd, "-ldl", "-lpthread");
#ifdef __APPLE__
            append_frameworks(&cmd);
#endif
            if (!nob_cmd_run_sync(cmd))
                nob_return_defer(false);
        }
    } break;
    case TARGET_WIN32_MINGW: {
        if (config.hotreload) {
            nob_log(NOB_ERROR, "TODO: hotreloading is not supported on %s yet",
                    NOB_ARRAY_GET(target_names, config.target));
            nob_return_defer(false);
        }
        // rc
        cmd.count = 0;
        nob_cmd_append(&cmd, "x86_64-w64-mingw32-windres");
        nob_cmd_append(&cmd, "./src/musicalizer.rc");
        nob_cmd_append(&cmd, "-O", "coff");
        nob_cmd_append(&cmd, "-o", "./build/musicalizer.res");

        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);

        // the only way to compile on windows for now
        cmd.count = 0;
        nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", dbg_option);
        nob_cmd_append(&cmd, "-I./raylib/src");
        // nob_cmd_append(&cmd, "-I./build/raylib-windows/include");
        nob_cmd_append(&cmd, "-o", "./build/musicalizer.exe");
        nob_cmd_append(&cmd, "./src/plug.c", "./src/ffmpeg_windows.c",
                       "./src/main.c", "./build/musicalizer.res");
        nob_cmd_append(
            &cmd, nob_temp_sprintf("./build/raylib/%s/libraylib.a",
                                   NOB_ARRAY_GET(target_names, config.target)));
        // nob_cmd_append(&cmd, "-L./build/raylib-windows/lib", "-lraylib");
        nob_cmd_append(&cmd, "-lwinmm", "-lgdi32");
        nob_cmd_append(&cmd, "-static");

        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
    } break;
    case TARGET_WIN32_MSVC: {
        if (config.hotreload) {
            nob_log(NOB_ERROR, "TODO: hotreloading is not supported on %s yet",
                    NOB_ARRAY_GET(target_names, config.target));
            nob_return_defer(false);
        }
        // rc
        cmd.count = 0;
        nob_cmd_append(&cmd, "x86_64-w64-mingw32-windres");
        nob_cmd_append(&cmd, "./src/musicalizer.rc");
        nob_cmd_append(&cmd, "-O", "coff");
        nob_cmd_append(&cmd, "-o", "./build/musicalizer.res");

        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);

        // the only way to compile on windows for now
        cmd.count = 0;
        nob_cmd_append(&cmd, "cl.exe");
        nob_cmd_append(&cmd, "/I", "./raylib/src");
        nob_cmd_append(&cmd, "-o", "/Fobuild\\", "/Febuild\\musicalizer.exe");
        nob_cmd_append(&cmd, "./src/plug.c", "./src/ffmpeg_windows.c",
                       "./src/main.c");
        // TODO: building resource file is not implemented for TARGET_WIN32_MSVC
        // "./build/musicalizer.res"
        nob_cmd_append(
            &cmd, "/link",
            nob_temp_sprintf("/LIBPATH:build/raylib/%s",
                             NOB_ARRAY_GET(target_names, config.target)),
            "raylib.lib");
        ;
        nob_cmd_append(&cmd, "Winmm.lib", "gdi32.lib", "User32.lib",
                       "Shell32.lib");

        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
    } break;

    default:
        NOB_ASSERT(0 && "unreachable");
    }

defer:
    nob_cmd_free(cmd);
    return result;
}

static const char *raylib_modules[] = {
    "rcore",   "raudio", "rglfw",     "rmodels",
    "rshapes", "rtext",  "rtextures", "utils",
};

bool build_raylib(Config config)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_File_Paths object_files = {0};

    if (!nob_mkdir_if_not_exists("./build/raylib")) {
        nob_return_defer(false);
    }

    Nob_Procs procs = {0};

    const char *build_path = nob_temp_sprintf(
        "./build/raylib/%s", NOB_ARRAY_GET(target_names, config.target));

    if (!nob_mkdir_if_not_exists(build_path)) {
        nob_return_defer(false);
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
        const char *input_path =
            nob_temp_sprintf("./raylib/src/%s.c", raylib_modules[i]);
        const char *output_path;

        switch (config.target) {
        case TARGET_POSIX:
        case TARGET_WIN32_MINGW:
            output_path =
                nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
            break;
        case TARGET_WIN32_MSVC:
            output_path =
                nob_temp_sprintf("%s/%s.obj", build_path, raylib_modules[i]);
            break;
        default:
            NOB_ASSERT(0 && "unreachable");
        }

        nob_da_append(&object_files, output_path);

        if (nob_needs_rebuild(output_path, &input_path, 1)) {
            cmd.count = 0;

            switch (config.target) {
            case TARGET_POSIX:
                nob_cmd_append(&cmd, "clang");
                nob_cmd_append(&cmd, dbg_option);
                nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP");
#ifdef __APPLE__
#else
                nob_cmd_append(&cmd, "-fPIC");
#endif // __APPLE__
                nob_cmd_append(&cmd, "-I./raylib/src/external/glfw/include");
                nob_cmd_append(&cmd, "-c", input_path);
                nob_cmd_append(&cmd, "-o", output_path);
#ifdef __APPLE__
                if (strcmp("rglfw", raylib_modules[i]) == 0)
                    nob_cmd_append(&cmd, "-ObjC");
#endif // __APPLE__
                break;
            case TARGET_WIN32_MINGW:
                nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
                nob_cmd_append(&cmd, dbg_option);
                nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP");
                nob_cmd_append(&cmd, "-fPIC");
                nob_cmd_append(&cmd, "-I./raylib/src/external/glfw/include");
                nob_cmd_append(&cmd, "-c", input_path);
                nob_cmd_append(&cmd, "-o", output_path);
                break;
            case TARGET_WIN32_MSVC:
                nob_cmd_append(&cmd, "cl.exe");
                nob_cmd_append(&cmd, "/DPLATFORM_DESKTOP");
                nob_cmd_append(&cmd, "/I",
                               "./raylib/src/external/glfw/include");
                nob_cmd_append(&cmd, "/c", input_path);
                nob_cmd_append(&cmd, nob_temp_sprintf("/Fo%s", output_path));
                break;
            default:
                NOB_ASSERT(0 && "unreachable");
            }

            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }

    cmd.count = 0;

    if (!nob_procs_wait(procs))
        nob_return_defer(false);

    switch (config.target) {
    case TARGET_POSIX:
    case TARGET_WIN32_MINGW:
        if (config.hotreload) {
            // hot reload
#ifdef __APPLE__
            const char *libraylib_path =
                nob_temp_sprintf("%s/libraylib.dylib", build_path);
#else
            const char *libraylib_path =
                nob_temp_sprintf("%s/libraylib.so", build_path);
#endif // __APPLE__

            if (nob_needs_rebuild(libraylib_path, object_files.items,
                                  object_files.count)) {
                if (config.target != TARGET_POSIX) {
                    nob_log(NOB_ERROR,
                            "TODO: dynamic raylib for %s is not supported yet",
                            NOB_ARRAY_GET(target_names, config.target));
                    nob_return_defer(false);
                }
                nob_cmd_append(&cmd, "clang");
#ifdef __APPLE__
                nob_cmd_append(&cmd, "-dynamiclib");
#else
                nob_cmd_append(&cmd, "-shared");
#endif // __APPLE__
                nob_cmd_append(&cmd, "-o", libraylib_path);
                for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
                    const char *input_path = nob_temp_sprintf(
                        "%s/%s.o", build_path, raylib_modules[i]);
                    nob_cmd_append(&cmd, input_path);
                }
#ifdef __APPLE__
                append_frameworks(&cmd);
#endif // __APPLE__
                if (!nob_cmd_run_sync(cmd))
                    nob_return_defer(false);
            }
        } else {
            // static
            const char *libraylib_path =
                nob_temp_sprintf("%s/libraylib.a", build_path);

            if (nob_needs_rebuild(libraylib_path, object_files.items,
                                  object_files.count)) {
                // #ifdef __APPLE__
                //         nob_cmd_append(&cmd, "libtool", "-static");
                //         nob_cmd_append(&cmd, "-o", libraylib_path);
                // #else
                nob_cmd_append(&cmd, "ar", "-crs", libraylib_path);
                // #endif // __APPLE__
                for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
                    const char *input_path = nob_temp_sprintf(
                        "%s/%s.o", build_path, raylib_modules[i]);
                    nob_cmd_append(&cmd, input_path);
                }
                if (!nob_cmd_run_sync(cmd))
                    nob_return_defer(false);
            }
        }
        break;

    case TARGET_WIN32_MSVC:
        if (config.hotreload) {
            nob_log(NOB_WARNING,
                    "TODO: dynamic raylib for %s is not supported yet",
                    NOB_ARRAY_GET(target_names, config.target));
            nob_return_defer(false);
        }
        const char *libraylib_path =
            nob_temp_sprintf("%s/raylib.lib", build_path);
        if (nob_needs_rebuild(libraylib_path, object_files.items,
                              object_files.count)) {
            nob_cmd_append(&cmd, "lib");
            for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
                const char *input_path = nob_temp_sprintf(
                    "%s/%s.obj", build_path, raylib_modules[i]);
                nob_cmd_append(&cmd, input_path);
            }
            nob_cmd_append(&cmd, nob_temp_sprintf("/OUT:%s", libraylib_path));
            if (!nob_cmd_run_sync(cmd))
                nob_return_defer(false);
        }
        break;
    default:
        NOB_ASSERT(0 && "unreachable");
    }

defer:
    nob_cmd_free(cmd);
    nob_da_free(object_files);
    return result;
}

void log_available_subcommands(const char *program, Nob_Log_Level level)
{
    nob_log(level, "Usage: %s <subcommand>", program);
    nob_log(level, "Subcommands:");
    nob_log(level, "    build");
    nob_log(level, "    config");
    nob_log(level, "    logo");
}

int main(int argc, char **argv)
{

    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        nob_log(NOB_ERROR, "No subcommand is provided");
        log_available_subcommands(program, NOB_ERROR);
        return 1;
    }

    const char *subcommand = nob_shift_args(&argc, &argv);

    if (strcmp(subcommand, "build") == 0) {
        Config config = {0};
        if (!load_config_from_file("./build/build.conf", &config)) {
            nob_log(NOB_ERROR,
                    "You may want to probably call `%s config` first", program);
            return 1;
        }
        nob_log(NOB_INFO, "------------------------------");
        log_config(config);
        nob_log(NOB_INFO, "------------------------------");
        if (!build_raylib(config))
            return 1;
        if (!build_main(config))
            return 1;
        if (config.target == TARGET_WIN32_MINGW ||
            config.target == TARGET_WIN32_MSVC) {
            if (!nob_copy_file("musicalizer-logged.bat",
                               "build/musicalizer-logged.bat"))
                return 1;
        }
        if (!nob_copy_directory_recursively("./resources", "./build/resources"))
            return 1;
    } else if (strcmp(subcommand, "config") == 0) {
        if (!nob_mkdir_if_not_exists("build"))
            return 1;
        Config config = {0};
        if (!parse_config_from_args(argc, argv, &config))
            return 1;
        if (config.help_requested) {
            nob_log(NOB_INFO, "Available config flags:");
            nob_log(NOB_INFO, "    -t <target>    set build target");
            nob_log(NOB_INFO, "    -r             enable hotreload");
            nob_log(NOB_INFO, "    -h             print this help");
            return 0;
        }
        nob_log(NOB_INFO, "------------------------------");
        log_config(config);
        nob_log(NOB_INFO, "------------------------------");
        if (!dump_config_to_file("./build/build.conf", config))
            return 1;
    } else if (strcmp(subcommand, "logo") == 0) {
        Nob_Procs procs = {0};

        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "convert");
        nob_cmd_append(&cmd, "-background", "None");
        nob_cmd_append(&cmd, "./resources/logo/logo.svg");
        nob_cmd_append(&cmd, "-resize", "256");

        nob_cmd_append(&cmd, "./resources/logo/logo-256.ico");

        nob_da_append(&procs, nob_cmd_run_async(cmd));

        cmd.count -= 1;

        nob_cmd_append(&cmd, "./resources/logo/logo-256.png");

        nob_da_append(&procs, nob_cmd_run_async(cmd));

        if (!nob_procs_wait(procs))
            return 1;
    } else {
        nob_log(NOB_ERROR, "Unknown subcommand %s", subcommand);
        log_available_subcommands(program, NOB_ERROR);
    }
    // TODO: subcommand for creating final distribution that we can ship to end
    // users. It should also supply the ffmpeg executable and stuff

    return 0;
}
