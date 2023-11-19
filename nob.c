#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "src/nob.h"

typedef enum {
    TARGET_LINUX,
    TARGET_WIN64_MINGW,
    TARGET_WIN64_MSVC,
    TARGET_MACOS,
    COUNT_TARGETS,
} Target;

const char *target_names[] = {
    [TARGET_LINUX] = "linux",
    [TARGET_WIN64_MINGW] = "win64-mingw",
    [TARGET_WIN64_MSVC] = "win64-msvc",
    [TARGET_MACOS] = "macos",
};
static_assert(4 == COUNT_TARGETS, "Amount of targets have changed");

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
    bool microphone;
} Config;

typedef struct {
    char major[2];
    char minor[2];
    char patch[3];
} Version;

bool compute_default_config(Config *config)
{
    memset(config, 0, sizeof(Config));
#ifdef _WIN32
#if defined(_MSC_VER)
    config->target = TARGET_WIN64_MSVC;
#else
    config->target = TARGET_WIN64_MINGW;
#endif
#else
#if defined(__APPLE__) || defined(__MACH__)
    config->target = TARGET_MACOS;
#else
    config->target = TARGET_LINUX;
#endif
#endif
    return true;
}

bool parse_config_from_args(int argc, char **argv, Config *config)
{

    while (argc > 0) {
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
        } else if (strcmp("-m", flag) == 0) {
            config->microphone = true;
        } else if (strcmp("-h", flag) == 0 || strcmp("--help", flag) == 0) {
            nob_log(NOB_INFO, "Available config flags:");
            nob_log(NOB_INFO, "    -t <target>    set build target");
            nob_log(NOB_INFO, "    -r             enable hotreload");
            nob_log(NOB_INFO, "    -m             enable microphone");
            nob_log(NOB_INFO, "    -h             print this help");
            return false;
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
    nob_log(NOB_INFO, "Microphone: %s",
            config.microphone ? "ENABLED" : "DISABLED");
}

bool dump_config_to_file(const char *path, Config config)
{
    Nob_String_Builder sb = {0};

    nob_log(NOB_INFO, "Saving configuration to %s", path);

    nob_sb_append_cstr(
        &sb, nob_temp_sprintf("target = %s" NOB_LINE_END,
                              NOB_ARRAY_GET(target_names, config.target)));
    nob_sb_append_cstr(&sb,
                       nob_temp_sprintf("hotreload = %s" NOB_LINE_END,
                                        config.hotreload ? "true" : "false"));
    nob_sb_append_cstr(&sb,
                       nob_temp_sprintf("microphone = %s" NOB_LINE_END,
                                        config.microphone ? "true" : "false"));
    bool res = nob_write_entire_file(path, sb.items, sb.count);
    nob_sb_free(sb);
    return res;
}

bool config_parse_boolean(const char *path, size_t row, Nob_String_View token,
                          bool *boolean)
{
    if (nob_sv_eq(token, nob_sv_from_cstr("true"))) {
        *boolean = true;
    } else if (nob_sv_eq(token, nob_sv_from_cstr("false"))) {
        *boolean = false;
    } else {
        nob_log(NOB_ERROR, "%s:%zu: Invalid boolean `" SV_Fmt "`", path,
                row + 1, SV_Arg(token));
        nob_log(NOB_ERROR, "Expected `true` or `false`");
        return false;
    }
    return true;
}

bool config_parse_target(const char *path, size_t row, Nob_String_View token,
                         Target *target)
{
    bool found = false;
    for (size_t t = 0; !found && t < COUNT_TARGETS; ++t) {
        if (nob_sv_eq(token, nob_sv_from_cstr(target_names[t]))) {
            *target = t;
            return true;
        }
    }
    nob_log(NOB_ERROR, "%s:%zu: Invalid target `" SV_Fmt "`", path, row + 1,
            SV_Arg(token));
    log_available_targets(NOB_ERROR);
    return false;
}

// not in the original version (used for macOS dylib install_name)
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

// not in the original (used for macOS linking)
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

bool load_config_from_file(const char *path, Config *config)
{
    bool result = true;
    Nob_String_Builder sb = {0};

    nob_log(NOB_INFO, "Loading configuration from %s", path);

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
            if (!config_parse_target(path, row, value, &config->target))
                nob_return_defer(false);
        } else if (nob_sv_eq(key, nob_sv_from_cstr("hotreload"))) {
            if (!config_parse_boolean(path, row, value, &config->hotreload))
                nob_return_defer(false);
        } else if (nob_sv_eq(key, nob_sv_from_cstr("microphone"))) {
            if (!config_parse_boolean(path, row, value, &config->microphone))
                nob_return_defer(false);
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

bool build_main(Config config)
{
    bool result = true;
    Nob_Cmd cmd = {0};

    switch (config.target) {
    case TARGET_MACOS: {

        // in case, you need files in ~/.local (here, headers)
        // const char *home = getenv("HOME");
        // nob_temp_sprintf("-I%s/.local/include", home);

        if (config.hotreload) {
            Nob_Procs procs = {0};

            // dynamic library case
            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", dbg_option);
            if (config.microphone)
                nob_cmd_append(&cmd, "-DFEATURE_MICROPHONE");
            nob_cmd_append(&cmd, "-I./raylib/src", "-I./src");
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
            nob_cmd_append(&cmd, "./src/plug.c", "./src/ffmpeg.c");
            nob_cmd_append(
                &cmd,
                nob_temp_sprintf("-L./build/raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)),
                "-lraylib");
            nob_cmd_append(&cmd, "-ldl", "-lpthread");
            append_frameworks(&cmd);
            nob_da_append(&procs, nob_cmd_run_async(cmd));

            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./raylib/src");
            nob_cmd_append(&cmd, "-DHOTRELOAD");
            nob_cmd_append(&cmd, "-o", "./build/musicalizer");
            nob_cmd_append(&cmd, "./src/main.c");
            nob_cmd_append(&cmd, "./src/hotreload.c");

            // TODO: -install_name @rpath/libraylib.dylib ?
            // I use DYLD_LIBRARY_PATH for now
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
            if (config.microphone)
                nob_cmd_append(&cmd, "-DFEATURE_MICROPHONE");
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
            append_frameworks(&cmd);
            if (!nob_cmd_run_sync(cmd))
                nob_return_defer(false);
        }
    } break;
    case TARGET_LINUX: {

        // in case, you need files in ~/.local (here, headers)
        // const char *home = getenv("HOME");
        // nob_temp_sprintf("-I%s/.local/include", home);

        if (config.hotreload) {
            Nob_Procs procs = {0};

            // dynamic library case
            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", dbg_option);
            if (config.microphone)
                nob_cmd_append(&cmd, "-DFEATURE_MICROPHONE");
            nob_cmd_append(&cmd, "-I./raylib/src", "-I./src");
            nob_cmd_append(&cmd, "-fPIC", "-shared", "-o",
                           "./build/libplug.so");
            nob_cmd_append(&cmd, "./src/plug.c", "./src/ffmpeg.c");
            nob_cmd_append(
                &cmd,
                nob_temp_sprintf("-L./build/raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)),
                "-lraylib");
            nob_cmd_append(&cmd, "-ldl", "-lpthread");
            nob_da_append(&procs, nob_cmd_run_async(cmd));

            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./raylib/src");
            nob_cmd_append(&cmd, "-DHOTRELOAD");
            nob_cmd_append(&cmd, "-o", "./build/musicalizer");
            nob_cmd_append(&cmd, "./src/main.c");
            nob_cmd_append(&cmd, "./src/hotreload.c");

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
            nob_cmd_append(
                &cmd,
                nob_temp_sprintf("-L./build/raylib/%s",
                                 NOB_ARRAY_GET(target_names, config.target)),
                "-lraylib");

            nob_da_append(&procs, nob_cmd_run_async(cmd));

            if (!nob_procs_wait(procs))
                nob_return_defer(false);

        } else {
            // static library case
            cmd.count = 0;
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", dbg_option);
            if (config.microphone)
                nob_cmd_append(&cmd, "-DFEATURE_MICROPHONE");
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
    case TARGET_WIN64_MINGW: {
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
        if (config.microphone)
            nob_cmd_append(&cmd, "-DFEATURE_MICROPHONE");
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
    case TARGET_WIN64_MSVC: {
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
        if (config.microphone)
            nob_cmd_append(&cmd, "-DFEATURE_MICROPHONE");
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
        case TARGET_MACOS:
        case TARGET_LINUX:
        case TARGET_WIN64_MINGW:
            output_path =
                nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
            break;
        case TARGET_WIN64_MSVC:
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
            case TARGET_MACOS:
                nob_cmd_append(&cmd, "clang");
                nob_cmd_append(&cmd, dbg_option);
                nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP");
                nob_cmd_append(&cmd, "-I./raylib/src/external/glfw/include");
                nob_cmd_append(&cmd, "-c", input_path);
                nob_cmd_append(&cmd, "-o", output_path);
                // IMPORTANT: you need Objective C for this one
                if (strcmp("rglfw", raylib_modules[i]) == 0)
                    nob_cmd_append(&cmd, "-ObjC");
                break;
            case TARGET_LINUX:
                nob_cmd_append(&cmd, "clang");
                nob_cmd_append(&cmd, dbg_option);
                nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP");
                nob_cmd_append(&cmd, "-fPIC");
                nob_cmd_append(&cmd, "-I./raylib/src/external/glfw/include");
                nob_cmd_append(&cmd, "-c", input_path);
                nob_cmd_append(&cmd, "-o", output_path);
                break;
            case TARGET_WIN64_MINGW:
                nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
                nob_cmd_append(&cmd, dbg_option);
                nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP");
                nob_cmd_append(&cmd, "-fPIC");
                nob_cmd_append(&cmd, "-I./raylib/src/external/glfw/include");
                nob_cmd_append(&cmd, "-c", input_path);
                nob_cmd_append(&cmd, "-o", output_path);
                break;
            case TARGET_WIN64_MSVC:
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
    case TARGET_MACOS:
    case TARGET_LINUX:
    case TARGET_WIN64_MINGW:
        if (config.hotreload) {
            // hot reload

            const char *libraylib_path = NULL;
            if (config.target == TARGET_MACOS) {
                // reminder: BSD was the first with dylibs
                libraylib_path =
                    nob_temp_sprintf("%s/libraylib.dylib", build_path);
            } else {
                libraylib_path =
                    nob_temp_sprintf("%s/libraylib.so", build_path);
            }

            if (nob_needs_rebuild(libraylib_path, object_files.items,
                                  object_files.count)) {
                if (config.target == TARGET_WIN64_MINGW) {
                    nob_log(NOB_ERROR,
                            "TODO: dynamic raylib for %s is not supported yet",
                            NOB_ARRAY_GET(target_names, config.target));
                    nob_return_defer(false);
                }
                nob_cmd_append(&cmd, "clang");
                if (config.target == TARGET_MACOS) {
                    nob_cmd_append(&cmd, "-dynamiclib");

                } else {
                    nob_cmd_append(&cmd, "-shared");
                }
                nob_cmd_append(&cmd, "-o", libraylib_path);
                for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
                    const char *input_path = nob_temp_sprintf(
                        "%s/%s.o", build_path, raylib_modules[i]);
                    nob_cmd_append(&cmd, input_path);
                }

                if (config.target == TARGET_MACOS) {
                    append_frameworks(&cmd);
                }

                if (!nob_cmd_run_sync(cmd))
                    nob_return_defer(false);
            }
        } else {
            // static
            const char *libraylib_path =
                nob_temp_sprintf("%s/libraylib.a", build_path);

            if (nob_needs_rebuild(libraylib_path, object_files.items,
                                  object_files.count)) {
                nob_cmd_append(&cmd, "ar", "-crs", libraylib_path);
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

    case TARGET_WIN64_MSVC:
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

bool build_dist(Config config)
{
    if (config.hotreload) {
        nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
        return false;
    }

    switch (config.target) {
    case TARGET_LINUX: {
        if (!nob_mkdir_if_not_exists("./musicalizer-linux-x86_64/"))
            return false;
        if (!nob_copy_file("./build/musicalizer",
                           "./musicalizer-linux-x86_64/musicalizer"))
            return false;
        if (!nob_copy_directory_recursively(
                "./resources/", "./musicalizer-linux-x86_64/resources/"))
            return false;
        // TODO: should we pack ffmpeg with Linux build?
        // There are some static executables for Linux
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "tar", "fvc", "./musicalizer-linux-x86_64.tar.gz",
                       "./musicalizer-linux-x86_64");
        bool ok = nob_cmd_run_sync(cmd);
        nob_cmd_free(cmd);
        if (!ok)
            return false;
    } break;

    case TARGET_WIN64_MINGW: {
        if (!nob_mkdir_if_not_exists("./musicalizer-win64-mingw/"))
            return false;
        if (!nob_copy_file("./build/musicalizer.exe",
                           "./musicalizer-win64-mingw/musicalizer.exe"))
            return false;
        if (!nob_copy_directory_recursively(
                "./resources/", "./musicalizer-win64-mingw/resources/"))
            return false;
        if (!nob_copy_file("musicalizer-logged.bat",
                           "./musicalizer-win64-mingw/musicalizer-logged.bat"))
            return false;
        // TODO: pack ffmpeg.exe with windows build
        // if (!nob_copy_file("ffmpeg.exe",
        // "./musicalizer-win64-mingw/ffmpeg.exe")) return false;
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "zip", "-r", "./musicalizer-win64-mingw.zip",
                       "./musicalizer-win64-mingw/");
        bool ok = nob_cmd_run_sync(cmd);
        nob_cmd_free(cmd);
        if (!ok)
            return false;
    } break;

    case TARGET_WIN64_MSVC: {
        nob_log(NOB_ERROR,
                "TODO: Creating distro for MSVC build is not implemented yet");
        return false;
    } break;

    case TARGET_MACOS: {
        nob_log(NOB_ERROR,
                "TODO: Creating distro for MacOS build is not implemented yet");
        return false;
    }

    default:
        NOB_ASSERT(0 && "unreachable");
    }

    return true;
}

void log_available_subcommands(const char *program, Nob_Log_Level level)
{
    nob_log(level, "Usage: %s <subcommand>", program);
    nob_log(level, "Subcommands:");
    nob_log(level, "    build (default)");
    nob_log(level, "    config");
    nob_log(level, "    dist");
    nob_log(level, "    svg");
    nob_log(level, "    help");
}

int main(int argc, char **argv)
{

    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);

    const char *subcommand = NULL;
    if (argc <= 0) {
        subcommand = "build";
    } else {
        subcommand = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcommand, "build") == 0) {
        Config config = {0};
        switch (nob_file_exists("./build/build.conf")) {
        case -1:
            return 1;
        case 0:
            if (!nob_mkdir_if_not_exists("build"))
                return 1;
            if (!compute_default_config(&config))
                return 1;
            if (!dump_config_to_file("./build/build.conf", config))
                return 1;
            break;
        case 1:
            if (!load_config_from_file("./build/build.conf", &config))
                return 1;
            break;
        }
        nob_log(NOB_INFO, "------------------------------");
        log_config(config);
        nob_log(NOB_INFO, "------------------------------");
        if (!build_raylib(config))
            return 1;
        if (!build_main(config))
            return 1;
        if (config.target == TARGET_WIN64_MINGW ||
            config.target == TARGET_WIN64_MSVC) {
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
        if (!compute_default_config(&config))
            return 1;
        if (!parse_config_from_args(argc, argv, &config))
            return 1;
        nob_log(NOB_INFO, "------------------------------");
        log_config(config);
        nob_log(NOB_INFO, "------------------------------");
        if (!dump_config_to_file("./build/build.conf", config))
            return 1;
    } else if (strcmp(subcommand, "dist") == 0) {
        Config config = {0};
        if (!load_config_from_file("./build/build.conf", &config))
            return 1;
        nob_log(NOB_INFO, "------------------------------");
        log_config(config);
        nob_log(NOB_INFO, "------------------------------");
        if (!build_dist(config))
            return 1;
    } else if (strcmp(subcommand, "svg") == 0) {
        Nob_Procs procs = {0};

        Nob_Cmd cmd = {0};

        if (nob_needs_rebuild1("./resources/logo/logo-256.ico",
                               "./resources/logo/logo.svg")) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "convert");
            nob_cmd_append(&cmd, "-background", "None");
            nob_cmd_append(&cmd, "./resources/logo/logo.svg");
            nob_cmd_append(&cmd, "-resize", "256");
            nob_cmd_append(&cmd, "./resources/logo/logo-256.ico");
            nob_da_append(&procs, nob_cmd_run_async(cmd));
        }

        if (nob_needs_rebuild1("./resources/logo/logo-256.png",
                               "./resources/logo/logo.svg")) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "convert");
            nob_cmd_append(&cmd, "-background", "None");
            nob_cmd_append(&cmd, "./resources/logo/logo.svg");
            nob_cmd_append(&cmd, "-resize", "256");
            nob_cmd_append(&cmd, "./resources/logo/logo-256.png");
            nob_da_append(&procs, nob_cmd_run_async(cmd));
        }

        if (nob_needs_rebuild1("./resources/icons/fullscreen.png",
                               "./resources/icons/fullscreen.svg")) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "convert");
            nob_cmd_append(&cmd, "-background", "None");
            nob_cmd_append(&cmd, "./resources/icons/fullscreen.svg");
            nob_cmd_append(&cmd, "./resources/icons/fullscreen.png");
            nob_da_append(&procs, nob_cmd_run_async(cmd));
        }

        if (nob_needs_rebuild1("./resources/icons/volume.png",
                               "./resources/icons/volume.svg")) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "convert");
            nob_cmd_append(&cmd, "-background", "None");
            nob_cmd_append(&cmd, "./resources/icons/volume.svg");
            nob_cmd_append(&cmd, "./resources/icons/volume.png");
            nob_da_append(&procs, nob_cmd_run_async(cmd));
        }

        if (!nob_procs_wait(procs))
            return 1;

    } else if (strcmp(subcommand, "help") == 0) {
        log_available_subcommands(program, NOB_INFO);

    } else {
        nob_log(NOB_ERROR, "Unknown subcommand %s", subcommand);
        log_available_subcommands(program, NOB_ERROR);
    }
    // TODO: it would be nice to check for situations like building
    // TARGET_WIN64_MSVC on Linux and report that it's not possible

    return 0;
}
