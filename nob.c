#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

typedef enum {
    TARGET_POSIX,
    TARGET_WIN32,
} Target;

const char *target_show(Target target)
{
    if (target == TARGET_POSIX)
        return "TARGET_POSIX";
    if (target == TARGET_WIN32)
        return "TARGET_WIN32";
    NOB_ASSERT(0 && "unreachable");
    return "(unknown)";
}

void target_compiler(Nob_Cmd *cmd, Target target)
{
    if (target == TARGET_WIN32) {
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
    } else {
        nob_cmd_append(cmd, "clang");
    }
}

void cflags(Nob_Cmd *cmd, Target target)
{
    nob_cmd_append(cmd, "-Wall", "-Wextra");
    // nob_cmd_append(cmd, "-g"); // I use lldb (w/ nvim-dap & lldb-vscode)
    if (target == TARGET_WIN32) {
        nob_cmd_append(cmd, "-I./build/raylib/include");
    } else {
        nob_cmd_append(cmd, "-I/Users/mars/.local/include");
        // nob_cmd_append(cmd, "-I/opt/homebrew/Cellar/raylib/4.5.0/include");
    }
}

void src(Nob_Cmd *cmd, Target target)
{
    nob_cmd_append(cmd, "./src/main.c");
    nob_cmd_append(cmd, "./src/plug.c");
    nob_cmd_append(cmd, "./src/separate_translation_unit_for_miniaudio.c");
    if (target == TARGET_WIN32) {
        nob_cmd_append(cmd, "./src/ffmpeg_windows.c");
    } else {
        nob_cmd_append(cmd, "./src/ffmpeg.c");
    }
}

void link_libraries(Nob_Cmd *cmd, Target target)
{
    if (target == TARGET_WIN32) {
        nob_cmd_append(cmd, "-L./build/raylib/lib");
        nob_cmd_append(cmd, "-lraylib", "-lwinmm", "-lgdi32", "-static");
    } else {
        nob_cmd_append(cmd, "-L/Users/mars/.local/lib");
        // nob_cmd_append(cmd, "-L/opt/homebrew/Cellar/raylib/4.5.0/lib");
        nob_cmd_append(cmd, "-lraylib", "-ldl", "-lpthread");
#ifdef __APPLE__
        // for miniaudio.h's MA_NO_RUNTIME_LINKING
        nob_cmd_append(cmd, "-framework", "CoreFoundation", "-framework",
                       "CoreAudio", "-framework", "AudioToolbox");
#endif // __APPLE__
    }
}

bool build_main_executable(const char *output_path, Target target)
{
    Nob_Cmd cmd = {0};

    target_compiler(&cmd, target);
    cflags(&cmd, target);
    nob_cmd_append(&cmd, "-o", "./build/musicalizer");
    src(&cmd, target);
    link_libraries(&cmd, target);

    nob_cmd_log(cmd);
    bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);

    return result;
}

int main(int argc, char **argv)
{

    const char *program = nob_shift_args(&argc, &argv);

    if (argc <= 0) {
        nob_log(NOB_INFO, "Usage: %s <subcommand>", program);
        nob_log(NOB_INFO, "Subcommands:");
        nob_log(NOB_INFO, "    build");
        nob_log(NOB_INFO, "    logo");
        nob_log(NOB_ERROR, "No subcommand is provided");
        return 1;
    }

    const char *subcommand = nob_shift_args(&argc, &argv);
    if (strcmp(subcommand, "build") == 0) {
#ifdef _WIN32
        Target target = TARGET_WIN32;
#else
        Target target = TARGET_POSIX;
#endif

        if (argc > 0) {
            const char *subcmd = nob_shift_args(&argc, &argv);
            if (strcmp(subcmd, "win32") == 0) {
                target = TARGET_WIN32;
            } else if (strcmp(subcmd, "posix") == 0) {
                target = TARGET_POSIX;
            } else {
                fprintf(stderr, "[ERROR] unknown subcommand %s\n", subcmd);
                return 1;
            }
        }

        nob_log(NOB_INFO, "TARGET: %s", target_show(target));

        if (!nob_mkdir_if_not_exists("build"))
            return 1;
        build_main_executable("./build/musicalizer", target);
        if (!nob_copy_directory_recursively("./resources/",
                                            "./build/resources/"))
            return 1;

        // batch file untested
        if (target == TARGET_WIN32) {
            if (!nob_copy_file("musicalizer-logged.bat",
                               "build/musicalizer-logged.bat"))
                return 1;
        }
    } else if (strcmp(subcommand, "logo") == 0) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "convert");
        nob_cmd_append(&cmd, "-background", "None");
        nob_cmd_append(&cmd, "./resources/logo/logo.svg");
        nob_cmd_append(&cmd, "-resize", "256");

        nob_cmd_append(&cmd, "./resources/logo/logo-256.ico");
        nob_cmd_log(cmd);
        if (!nob_cmd_run_sync(cmd))
            return 1;

        cmd.count -= 1;

        nob_cmd_append(&cmd, "./resources/logo/logo-256.png");
        nob_cmd_log(cmd);
        if (!nob_cmd_run_sync(cmd))
            return 1;
    }

    return 0;
}
