#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0600     // Change this to the appropriate value to target other versions of Windows.
#endif

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <tchar.h>
#include "Display.h"
#include "Menu.h"
#include "IO.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    /* Ensure command is good */
    LPWSTR *argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLine(), &argc);

    /* Ensure command is good */
    if( argc < 2 )
    {
        MessageBox(
            NULL,
            (LPCWSTR)L"Missing ini file argument!",
            (LPCWSTR)L"Invalid Invocation",
            MB_ICONERROR | MB_OK | MB_DEFBUTTON1
        );
        return 1;
    }

    if( argc > 2 )
    {
        MessageBox(
            NULL,
            (LPCWSTR)L"Too many arguments specified!",
            (LPCWSTR)L"Invalid Invocation",
            MB_ICONERROR | MB_OK | MB_DEFBUTTON1
        );
        return 1;
    }

    // Initialize the IO
    IO *io = new IO();
    if (!io->Ready())
    {
        // Failed to initialize, give up
        delete io;

        return 1;
    }

    // Initialize the menu
    Menu *menu = new Menu(argv[1]);
    if( menu->NumberOfEntries() < 1 )
    {
        MessageBox(
            NULL,
            (LPCWSTR)L"No games configured to launch!",
            (LPCWSTR)L"Invalid Invocation",
            MB_ICONERROR | MB_OK | MB_DEFBUTTON1
        );

        delete menu;
        delete io;

        return 1;
    }

    /* Create menu screen */
    Display *display = new Display(hInstance, io, menu);

    /* Actual game to load */
    char *path = NULL;

    /* It may have taken a long time to init */
    menu->ResetTimeout();

    // Input loop
    while(true) {
        io->Tick();
        menu->Tick();
        display->Tick();

        /* See if somebody killed the display window */
        if (display->WasClosed())
        {
            break;
        }

        /* Check to see if we ran out of time waiting for input, and to see
           if the user confirmed a selection */
        if (
            menu->ShouldBootDefault() ||
            io->ButtonPressed(BUTTON_1P_START) ||
            io->ButtonPressed(BUTTON_2P_START)
        ) {
            int entry = display->GetSelectedItem();
            path = menu->GetEntryPath(entry);
            break;
        }

        /* Update lights blinking so people know they can use the menu */
        if (menu->SecondsLeft() & 1)
        {
            io->LightOn(LIGHT_1P_MENU);
            io->LightOn(LIGHT_2P_MENU);
        }
        else
        {
            io->LightOff(LIGHT_1P_MENU);
            io->LightOff(LIGHT_2P_MENU);
        }
    }

    // Close and free libraries
    delete display;
    delete menu;
    delete io;

    if (path != NULL)
    {
        /* Crate a batch file that launches the game after we exit */
        char tempPath[1024];
        GetTempPathA(1024, tempPath);
        strncat_s(tempPath, 1024, "/launch.bat", 11);
        HANDLE hBat = CreateFileA(tempPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        char command[MAX_GAME_LOCATION_LENGTH + 128];
        sprintf_s(command, MAX_GAME_LOCATION_LENGTH + 128, "ping 127.0.0.1 -n 5 -w 1000 > nul\r\n%s\r\n", path);
        DWORD bytesWritten;
        WriteFile(hBat, command, strlen(command), &bytesWritten, NULL);
        CloseHandle(hBat);

        wchar_t* wCommand = new wchar_t[4096];
        MultiByteToWideChar(CP_ACP, 0, tempPath, -1, wCommand, 4096);

        STARTUPINFO info={sizeof(info)};
        PROCESS_INFORMATION processInfo;
        CreateProcess(NULL, wCommand, NULL, NULL, FALSE, 0, NULL, NULL, &info, &processInfo);

        delete wCommand;
    }

    // All done
    return 0;
}
