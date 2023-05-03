#include "clither/signals.h"
#include "clither/log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdio.h>
#include <io.h>

/* ------------------------------------------------------------------------- */
void
signals_install(void)
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        int fd0, fd1, fd2;
        fd0 = _open_osfhandle((intptr_t)GetStdHandle(STD_INPUT_HANDLE), 0);
        fd1 = _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), 0);
        fd2 = _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), 0);
        _dup2(fd0, 0);
        _dup2(fd1, 1);
        _dup2(fd2, 2);

        log_dbg("Attached to console\n");
    }
    else
    {
        HANDLE hStdout, hStdin;

        AllocConsole();

        /* Redirect the CRT standard input, output, and error handles to the console */
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stderr);
        freopen("CONOUT$", "w", stdout);

        /* Note that there is no CONERR$ file */
        hStdout = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        hStdin = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        SetConsoleMode(hStdin, ENABLE_WINDOW_INPUT);
        SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        SetStdHandle(STD_OUTPUT_HANDLE, hStdout);
        SetStdHandle(STD_ERROR_HANDLE, hStdout);
        SetStdHandle(STD_INPUT_HANDLE, hStdin);

        log_dbg("Allocated console\n");
    }
}

/* ------------------------------------------------------------------------- */
void
signals_remove(void)
{
}

/* ------------------------------------------------------------------------- */
char
signals_exit_requested(void)
{
    int i;
    DWORD dwNumRead;
    INPUT_RECORD irInBuf[1];
    HANDLE hStdin;

    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (!ReadConsoleInput(hStdin, irInBuf, 1, &dwNumRead))
        return 1;

    for (i = 0; i < dwNumRead; ++i)
    {
        switch (irInBuf[i].EventType)
        {
        case KEY_EVENT: {
            const KEY_EVENT_RECORD* k = &irInBuf[i].Event.KeyEvent;
            if (k->bKeyDown && (k->dwControlKeyState & LEFT_CTRL_PRESSED) && k->wVirtualKeyCode == 0x43 /* C */)
                return 1;
        } break;
        case MOUSE_EVENT:
        case WINDOW_BUFFER_SIZE_EVENT:
        case FOCUS_EVENT:
        case MENU_EVENT:
            break;
        }
    }

    return 0;
}
