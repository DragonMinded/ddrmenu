#include <stdio.h>
#include <windows.h>

#include "Display.h"
#include "Menu.h"
#include "IO.h"

Menu *globalMenu;
int globalResX, globalResY;
bool globalQuit;
unsigned int globalSelected;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_CLOSE:
            DestroyWindow(hwnd);
			globalQuit = true;
			return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
			globalQuit = true;
			return 0;
		case WM_QUIT:
			globalQuit = true;
			return 0;
		case WM_PAINT:
            /* Set up double buffer */
			PAINTSTRUCT ps;
			HDC windowHdc = BeginPaint(hwnd, &ps);
            HDC hdc = CreateCompatibleDC(windowHdc);
            HBITMAP Membitmap = CreateCompatibleBitmap(windowHdc, globalResX, globalResY);
            SelectObject(hdc, Membitmap);

			/* Paint the window background */
			HBRUSH background = CreateSolidBrush(RGB(0,0,0));
            FillRect(hdc, &ps.rcPaint, background);
			DeleteObject(background);

			/* Set up text display */
			SetTextColor(hdc, RGB(240, 240, 240));
			SetBkMode(hdc, TRANSPARENT);
			SetBkColor(hdc, RGB(24, 24, 24));
			HFONT hFont = CreateFont(FONT_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, L"Verdana");
			SelectObject(hdc, hFont);

			/* Set up brush colors */
			SelectObject(hdc, GetStockObject(DC_PEN));
            SelectObject(hdc, GetStockObject(DC_BRUSH));
            SetDCBrushColor(hdc, RGB(0,0,0));
            SetDCPenColor(hdc, RGB(255,255,255));
        
			/* Draw each menu item */
			for( unsigned int i = 0; i < globalMenu->NumberOfEntries(); i++ ) {
				unsigned int top = ((ITEM_HEIGHT + ITEM_PADDING) * i) + ITEM_PADDING;
				unsigned int bottom = top + ITEM_HEIGHT;
				unsigned int left = ITEM_PADDING;
				unsigned int right = globalResX - ITEM_PADDING;
				
				/* Draw bounding rectangle */
                if (i == globalSelected)
                {
                    SetDCPenColor(hdc, RGB(255,255,255));
                }
                else
                {
                    SetDCPenColor(hdc, RGB(0,0,0));
                }
				Rectangle(hdc, left, top, right, bottom);

				/* Draw text */
				RECT rect;
				rect.top = top;
				rect.bottom = bottom;
				rect.left = left;
				rect.right = right;
				
				wchar_t* wString = new wchar_t[4096];
				MultiByteToWideChar(CP_ACP, 0, globalMenu->GetEntryName(i), -1, wString, 4096);
				DrawText(hdc, wString, -1, &rect, DT_SINGLELINE | DT_NOCLIP | DT_CENTER | DT_VCENTER);
				delete wString;
        	}

			DeleteObject(hFont);
			
            /* Copy double-buffer over */
            BitBlt(windowHdc, 0, 0, globalResX, globalResY, hdc, 0, 0, SRCCOPY);
            DeleteObject(Membitmap);
            DeleteDC(hdc);
            DeleteDC(windowHdc);

            EndPaint(hwnd, &ps);
			return 0;
	}

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GetDesktopResolution(int& horizontal, int& vertical)
{
   RECT desktop;
   const HWND hDesktop = GetDesktopWindow();
   GetWindowRect(hDesktop, &desktop);
   horizontal = desktop.right;
   vertical = desktop.bottom;
}

Display::Display(HINSTANCE hInstance, IO *ioInst, Menu *mInst)
{
	inst = hInstance;
	globalMenu = mInst;

	// Register the callback
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = inst;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

	// Get window sizes
	GetDesktopResolution(globalResX, globalResY);
	globalQuit = false;
    globalSelected = 0;
    selected = 0;
    menu = mInst;
    io = ioInst;

	// Create an empty window
	hwnd = CreateWindow(CLASS_NAME, 0, WS_BORDER, 0, 0, globalResX, globalResY, NULL, NULL, inst, NULL);
	LONG lStyle = GetWindowLong(hwnd, GWL_STYLE);
	lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
	SetWindowLong(hwnd, GWL_STYLE, lStyle);
	LONG lExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
	SetWindowLong(hwnd, GWL_EXSTYLE, lExStyle);

	/* Display it */
	SetWindowPos(hwnd, NULL, 0,0,0,0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
	ShowCursor(false);
}

Display::~Display(void)
{
	ShowCursor(true);
	DestroyWindow(hwnd);
	UnregisterClass(CLASS_NAME, inst);
}

void Display::Tick(void)
{
    /* Firat, handle inputs */
    if (io->ButtonPressed(BUTTON_1P_MENULEFT) || io->ButtonPressed(BUTTON_2P_MENULEFT))
    {
        if (selected > 0)
        {
            selected --;
        }
    }
    if (io->ButtonPressed(BUTTON_1P_MENURIGHT) || io->ButtonPressed(BUTTON_2P_MENURIGHT))
    {
        if (selected < (menu->NumberOfEntries() - 1))
        {
            selected ++;
        }
    }

    /* Now, handle whether we should repaint */
    if (globalSelected != selected)
    {
        globalSelected = selected;
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
    }

    /* Now, handle repainting */
    MSG msg = { };
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
	}
}

bool Display::WasClosed()
{
	return globalQuit;
}

unsigned int Display::GetSelectedItem()
{
    return selected;
}