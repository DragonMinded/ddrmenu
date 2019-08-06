#pragma once

#include <stdio.h>
#include <windows.h>

#include "Menu.h"
#include "IO.h"

#define CLASS_NAME L"DDR Cabinet Button Launcher"

#define ITEM_HEIGHT 40
#define ITEM_PADDING 10
#define FONT_SIZE 18

class Display
{
public:
    Display(HINSTANCE hInstance, IO *io, Menu *mInst);
    ~Display();

    void Tick();
    bool WasClosed();

    unsigned int GetSelectedItem();

private:
    HINSTANCE inst;
    HWND hwnd;

    Menu *menu;
    IO *io;

    unsigned int selected;
};
