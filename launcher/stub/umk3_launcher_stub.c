/* UMK3-Launcher.exe in the game folder -- a few kilobytes that start the real
 * launcher, launcher\bin\UMK3-Launcher.exe, and leave.
 *
 * The launcher is Electron. Built as one "portable" .exe it unpacked about
 * 270 MB to a temp folder at every start: 9.3 s before the window showed on
 * Mary's machine. Shipped unpacked under launcher\bin it shows in 0.7 s. This
 * stub keeps the one double-clickable file at the top of the game folder.
 *
 * It tells the launcher where the game is (UMK3_ROOT, and the working folder),
 * passes its own arguments on, and touches nothing else. */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    wchar_t dir[MAX_PATH], exe[MAX_PATH + 64], *cmd, *slash;
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    size_t n;

    (void)inst; (void)prev; (void)show;
    if (!GetModuleFileNameW(NULL, dir, MAX_PATH))
        return 1;
    slash = wcsrchr(dir, L'\\');
    if (slash)
        *slash = 0;
    swprintf(exe, sizeof exe / sizeof exe[0],
             L"%ls\\launcher\\bin\\UMK3-Launcher.exe", dir);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"Falta launcher\\bin\\UMK3-Launcher.exe.\n"
                          L"Vuelve a descomprimir el port completo.\n\n"
                          L"launcher\\bin\\UMK3-Launcher.exe is missing.\n"
                          L"Unpack the whole port again.",
                    L"UMK3 Launcher", MB_ICONERROR);
        return 1;
    }
    SetEnvironmentVariableW(L"UMK3_ROOT", dir);

    n = wcslen(exe) + wcslen(args) + 8;
    cmd = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, n * sizeof(wchar_t));
    if (!cmd)
        return 1;
    swprintf(cmd, n, L"\"%ls\" %ls", exe, args);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi))
        return 1;
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
