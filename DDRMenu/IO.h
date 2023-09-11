#pragma once

#include <tchar.h>
#include <windows.h>

// Button definitions
#define BUTTON_1P_UP 0x0001
#define BUTTON_1P_DOWN 0x0002
#define BUTTON_1P_LEFT 0x0004
#define BUTTON_1P_RIGHT 0x0008

#define BUTTON_2P_UP 0x0010
#define BUTTON_2P_DOWN 0x0020
#define BUTTON_2P_LEFT 0x0040
#define BUTTON_2P_RIGHT 0x0080

#define BUTTON_1P_MENUUP 0x0100
#define BUTTON_1P_MENUDOWN 0x0200
#define BUTTON_1P_MENULEFT 0x0400
#define BUTTON_1P_MENURIGHT 0x0800

#define BUTTON_2P_MENUUP 0x1000
#define BUTTON_2P_MENUDOWN 0x2000
#define BUTTON_2P_MENULEFT 0x4000
#define BUTTON_2P_MENURIGHT 0x8000

#define BUTTON_1P_START 0x10000
#define BUTTON_2P_START 0x20000

#define BUTTON_TEST 0x100000
#define BUTTON_SERVICE 0x200000
#define BUTTON_COIN 0x400000

// Cabinet return type definitions
#define CABINET_UNKNOWN 0
#define CABINET_SD 1
#define CABINET_HD 2

// Lights definitions
#define LIGHT_1P_MENU 0x01000000
#define LIGHT_2P_MENU 0x02000000

#define LIGHT_MARQUEE_LOWER_RIGHT 0x10000000
#define LIGHT_MARQUEE_UPPER_RIGHT 0x20000000
#define LIGHT_MARQUEE_LOWER_LEFT 0x40000000
#define LIGHT_MARQUEE_UPPER_LEFT 0x80000000

#define LIGHT_1P_RIGHT 0x00000008
#define LIGHT_1P_LEFT 0x00000010
#define LIGHT_1P_DOWN 0x00000020
#define LIGHT_1P_UP 0x00000040

#define LIGHT_2P_RIGHT 0x00000800
#define LIGHT_2P_LEFT 0x00001000
#define LIGHT_2P_DOWN 0x00002000
#define LIGHT_2P_UP 0x00004000

#define LIGHT_BASS_NEONS 0x00400000

//time for watchdog to be reset every time
#define WATCHDOG_FEED_S 180

typedef struct {
    unsigned int slot1;
    unsigned int slot2;
} coincount;

class IO
{
public:
    IO();
    ~IO();

    bool Ready();
    void Tick();
    unsigned int ButtonsPressed();
    bool ButtonPressed(unsigned int button);
    unsigned int ButtonsHeld();
    bool ButtonHeld(unsigned int button);
    void SetLights(unsigned int lights);
    void LightOn(unsigned int light);
    void LightOff(unsigned int light);
    void FeedWatchdog();
private:
    HANDLE p3io;
    HANDLE extio;

    unsigned int ExchangeP3IO(unsigned char *outbuf, unsigned int outlen, unsigned char *inbuf, unsigned int inlen);
    bool ExchangeEXTIO(unsigned int message);

    void GetVersion();
    void SetMode();
    unsigned int GetCabType(unsigned int request);
    coincount GetCoinstock();
    unsigned int GetButtonsHeld();

    bool is_ready;
    unsigned int sequence;
    unsigned int buttons;
    unsigned int lastButtons;
    unsigned int lastcablights;
    unsigned int lastpadlights;
};
