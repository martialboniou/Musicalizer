#include "errhandlingapi.h"
#include "handleapi.h"
#include "minwinbase.h"
#include "processenv.h"
#include "processthreadsapi.h"
#include "synchapi.h"
#include "winnt.h"
#include <assert.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef HANDLE Pid;
typedef HANDLE Fd;

LPSTR GetLastErrorAsString()
{
    DWORD errorMessageId = GetLastError();
    assert(errorMessageId != 0);

    LPSTR messageBuffer = NULL;

    DWORD size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);

    return messageBuffer;
}

int main()
{
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    BOOL bSuccess = CreateProcess(NULL, "ffmpeg.exe", NULL, NULL, TRUE, 0, NULL,
                                  NULL, &siStartInfo, &piProcInfo);

    if (!bSuccess) {
        fprintf(stderr, "ERROR: Could not create child process %s\n",
                GetLastErrorAsString());
        return 1;
    }

    CloseHandle(piProcInfo.hThread);

    HANDLE pid = piProcInfo.hProcess;

    DWORD result = WaitForSingleObject(pid,     // HANDLE hHandle,
                                       INFINITE // DWORD dwMilliseconds
    );

    if (result == WAIT_FAILED) {
        fprintf(stderr, "Could not wait on child process: %s\n",
                GetLastErrorAsString());
        return 1;
    }

    DWORD exit_status;
    if (GetExitCodeProcess(pid, &exit_status) == 0) {
        fprintf(stderr, "Could not get process exit code: %lu\n",
                GetLastError());
        return 1;
    }

    if (exit_status != 0) {
        fprintf(stderr, "Command exited with exit code %lu\n", exit_status);
        return 1;
    }

    CloseHandle(pid);

    return 0;
}
