#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>

#include "IO.h"

/* P3IO GUID as reversed out of a ddr.dll */
DEFINE_GUID(P3IO_GUID, 0x1FA4A480, 0xAC60, 0x40C7, 0xA7, 0xAC, 0x52, 0x79, 0x0F, 0x34, 0x57, 0x5A);

IO::IO()
{
    /* Start with not being ready */
    is_ready = false;

    /* Try to find and initialize the P3IO */
    HDEVINFO devinfo = SetupDiGetClassDevsW(&P3IO_GUID, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (devinfo == (HDEVINFO)-1)
    {
        /* Failed to grab initial handle */
        fprintf(stderr, "Failed to gather P3IO devices!\n");
        return;
    }

    SP_DEVICE_INTERFACE_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(deviceInfoData);
    if( !SetupDiEnumDeviceInterfaces(devinfo, 0, &P3IO_GUID, 0, &deviceInfoData) )
    {
        /* Failed to enumerate interfaces */
        fprintf(stderr, "Failed to enumerate P3IO devices!\n");
        SetupDiDestroyDeviceInfoList(devinfo);
        return;
    }

    DWORD requiredSize;
    SetupDiGetDeviceInterfaceDetailW(devinfo, &deviceInfoData, 0, 0, &requiredSize, 0);
    SP_DEVICE_INTERFACE_DETAIL_DATA *detailData = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(requiredSize);
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if( !SetupDiGetDeviceInterfaceDetailW(devinfo, &deviceInfoData, detailData, requiredSize, 0, 0) )
    {
        /* Failed to get interface detail */
        fprintf(stderr, "Failed to get interface details!\n");
        free(detailData);
        SetupDiDestroyDeviceInfoList(devinfo);
        return;
    }

    /* Create filename so we can open the P3IO */
    wchar_t filename[256];
    wcscpy_s(filename, 256, detailData->DevicePath);
    wcscat_s(filename, 256, L"\\p3io");

    /* Now, open the file */
    p3io = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (p3io == INVALID_HANDLE_VALUE)
    {
        /* Failed to get interface detail */
        fprintf(stderr, "Failed to open P3IO!\n");
        free(detailData);
        SetupDiDestroyDeviceInfoList(devinfo);
        return;
    }

    /* Successfully opened P3IO! */
    free(detailData);
    SetupDiDestroyDeviceInfoList(devinfo);

    /* Now, optionally initialize the EXTIO. This gets us control over
       the foot panel lights and bass neons. */
    extio = CreateFileA("COM1", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (extio != INVALID_HANDLE_VALUE)
    {
        /* Set up standard params based on game DLL */
        DCB params = {0};
        params.DCBlength = sizeof(params);
        params.BaudRate = CBR_38400;
        params.ByteSize = 8;
        params.StopBits = ONESTOPBIT;
        params.Parity = NOPARITY;

        /* Copypasta that works */
        params.fOutxCtsFlow = 0;
        params.fOutxDsrFlow = 0;
        params.fDtrControl = DTR_CONTROL_DISABLE;
        params.fDsrSensitivity = 0;
        params.fOutX = 0;
        params.fInX = 0;
        params.fRtsControl = RTS_CONTROL_DISABLE;

        /* Set up the COM1 params */
        SetCommState(extio, &params);

        COMMTIMEOUTS timeouts = { 0 };
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutConstant = 0;
        timeouts.ReadIntervalTimeout = -1;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutMultiplier = 100;

        /* Set up read timeouts */
        SetCommTimeouts(extio, &timeouts);

        /* Start fresh */
        PurgeComm( extio, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR );
    }

    /* Looks good! */
    is_ready = true;
    sequence = 0;
    buttons = 0;
    lastButtons = 0;
    lastpadlights = 0xFFFFFFFF;
    lastcablights = 0xFFFFFFFF;

    /* Now, perform init sequence */
    GetVersion();
    SetMode();
    GetCabType(1);
    GetCoinstock();
    GetCabType(0);
    SetLights(0);
    FeedWatchdog();
}

IO::~IO()
{
    if (!is_ready) { return; }

    // Kill the handle to the extio
    if (extio != INVALID_HANDLE_VALUE)
    {
        CloseHandle(extio);
    }

    //clear lights on exit.
    SetLights(0);

    //feed the dog to give the next program the most amount of time to continue feeding it.
    FeedWatchdog();

    // Kill the handle to the file that we have
    CloseHandle(p3io);
    is_ready = false;
}

bool IO::Ready()
{
    return is_ready;
}

void IO::GetVersion()
{
    unsigned char outbuf[1] = { 0x01 };
    unsigned char inbuf[20] = { 0x00 };
    unsigned int actual = ExchangeP3IO(outbuf, 1, inbuf, 20);

    // Don't need to do anything with the version response. On actual
    // cabinets, this is 8 bytes long and looks similar to this response:
    // 0x01 0x47 0x33 0x32 0x00 0x02 0x02 0x06
    if (actual == 0)
    {
        fprintf(stderr, "Got unexpected size %d back from version get request!\n", actual);
    }
    else if (inbuf[0] != outbuf[0])
    {
        fprintf(stderr, "Got unexpected response from version get request!\n");
    }
}

void IO::SetMode()
{
    unsigned char outbuf[1] = { 0x2F };
    unsigned char inbuf[2] = { 0x00 };
    unsigned int actual = ExchangeP3IO(outbuf, 1, inbuf, 2);

    // Don't care about response, only log if we got incorrect mode length.
    // On actual cabinets this is 2 bytes long and looks similar to this:
    // 0x2F 0x00
    if (actual != 2)
    {
        fprintf(stderr, "Got unexpected size %d back from mode set request!\n", actual);
    }
    else if (inbuf[0] != outbuf[0])
    {
        fprintf(stderr, "Got unexpected response from mode set request!\n");
    }
}

unsigned int IO::GetCabType(unsigned int request)
{
    unsigned char outbuf[2] = { 0x27, request };
    unsigned char inbuf[2] = { 0x00 };
    unsigned int actual = ExchangeP3IO(outbuf, 2, inbuf, 2);

    if (actual == 2)
    {
        if (inbuf[0] != outbuf[0])
        {
            fprintf(stderr, "Got unexpected response from cab type get request!\n");
            return 0;
        }
        else
        {
            return inbuf[1];
        }
    }
    else
    {
        fprintf(stderr, "Got unexpected size %d back from cab type get request!\n", actual);
        return 0;
    }
}

void IO::FeedWatchdog()
{
    //cmd 0x05 "watchdog set time in sec", parameter in sec.
    unsigned char outbuf[2] = { 0x05, WATCHDOG_FEED_S };
    unsigned char inbuf[2] = { 0x00 };
    unsigned int actual = ExchangeP3IO(outbuf, 2, inbuf, 2);

    if (actual != 2)
    {
        //check to make sure the p3io responds
        fprintf(stderr, "Got unexpected size %d back from watchdog request!\n", actual);
    }
    else if (inbuf[0] != outbuf[0])
    {
        //check to make sure the p3io responds to the same cmd we sent it.
        fprintf(stderr, "Got unexpected response from watchdog request!\n");
    }
}

void IO::SetLights(unsigned int lights)
{
    /* Mask off the lights bits for each part */
    unsigned int cablights = lights & (
        LIGHT_1P_MENU |
        LIGHT_2P_MENU |
        LIGHT_MARQUEE_LOWER_RIGHT |
        LIGHT_MARQUEE_UPPER_RIGHT |
        LIGHT_MARQUEE_LOWER_LEFT |
        LIGHT_MARQUEE_UPPER_LEFT
    );
    unsigned int padlights = lights & (
        LIGHT_1P_RIGHT |
        LIGHT_1P_LEFT |
        LIGHT_1P_DOWN |
        LIGHT_1P_UP |
        LIGHT_2P_RIGHT |
        LIGHT_2P_LEFT |
        LIGHT_2P_DOWN |
        LIGHT_2P_UP |
        LIGHT_BASS_NEONS
    );

    /* First, talk to the P3IO for cab lights */
    if (lastcablights != cablights)
    {
        unsigned char outbuf[6] = { 0x24, 0xFF, cablights & 0xFF, (cablights >> 8) & 0xFF, (cablights >> 16) & 0xFF, (cablights >> 24) & 0xFF };
        unsigned char inbuf[3] = { 0x00 };
        unsigned int actual = ExchangeP3IO(outbuf, 6, inbuf, 3);

        if (actual == 3)
        {
            if (inbuf[0] != outbuf[0])
            {
                fprintf(stderr, "Got unexpected response from lights set request!\n");
            }
        }
        else
        {
            fprintf(stderr, "Got unexpected size %d back from lights set request!\n", actual);
        }

        lastcablights = cablights;
    }

    /* Now, talk to the EXTIO for pad lights */
    if (lastpadlights != padlights)
    {
        ExchangeEXTIO(padlights);
        lastpadlights = padlights;
    }

    //we update the lights ~1s, so this is a good timer to feed our lovely watchdog.
    FeedWatchdog();
}

coincount IO::GetCoinstock()
{
    unsigned char outbuf[1] = { 0x31 };
    unsigned char inbuf[6] = { 0x00 };
    unsigned int actual = ExchangeP3IO(outbuf, 1, inbuf, 6);

    /* The game checks to make sure that byte 1 is zero */
    if (actual == 6)
    {
        if (inbuf[0] == outbuf[0] && inbuf[1] == 0)
        {
            /* From game RE, it looks like bytes 2-3, and bytes 4-5 are the 1P and 2P coinstock */
            coincount count;
            count.slot1 = inbuf[2] << 8 | inbuf[3];
            count.slot2 = inbuf[4] << 8 | inbuf[5];
            return count;
        }
        else
        {
            fprintf(
                stderr,
                "Got unexpected data %02X %02X %02X %02X %02X %02X back from coinstock get request!\n",
                inbuf[0],
                inbuf[1],
                inbuf[2],
                inbuf[3],
                inbuf[4],
                inbuf[5]
            );

            coincount count;
            count.slot1 = 0;
            count.slot2 = 0;
            return count;
        }
    }
    else
    {
        fprintf(stderr, "Got unexpected size %d back from coinstock get request!\n", actual);

        coincount count;
        count.slot1 = 0;
        count.slot2 = 0;
        return count;
    }
}

unsigned int IO::ExchangeP3IO(unsigned char *outbuf, unsigned int outlen, unsigned char *inbuf, unsigned int inlen)
{
    if (!is_ready)
    {
        return 0;
    }

    /* Malloc enough room for every byte to be escaped if needed */
    unsigned char *realoutbuf = (unsigned char *)malloc((outlen * 2) + 2);
    unsigned int loc = 0;

    realoutbuf[loc++] = 0xAA;
    realoutbuf[loc++] = outlen + 1;
    realoutbuf[loc++] = (sequence++) & 0xF;

    for (unsigned int i = 0; i < outlen; i++)
    {
        if (outbuf[i] == 0xAA || outbuf[i] == 0xFF)
        {
            /* Escape this byte */
            realoutbuf[loc++] = 0xFF;
            realoutbuf[loc++] = ~outbuf[i];
        }
        else
        {
            /* Output directly */
            realoutbuf[loc++] = outbuf[i];
        }
    }

    /* Write it out */
    DWORD actual = 0;
    WriteFile(p3io, realoutbuf, loc, &actual, 0);
    FlushFileBuffers(p3io);
    free(realoutbuf);

    if (actual != loc)
    {
        fprintf(stderr, "Failed to output correct amount of data to P3IO!\n");
        return 0;
    }

    /* Read in the response */
    unsigned char *realinbuf = (unsigned char *)malloc((inlen * 2) + 64);
    ReadFile(p3io, realinbuf, (inlen * 2) + 64, &actual, 0);
    loc = 0;

    while (true)
    {
        if (loc >= actual)
        {
            /* Never found the packet */
            fprintf(stderr, "Failed to find SOM in response from P3IO!\n");
            free(realinbuf);
            return 0;
        }

        if (realinbuf[loc++] == 0xAA)
        {
            /* Got SOM */
            break;
        }
    }

    if (loc >= actual)
    {
        /* Not enough data */
        fprintf(stderr, "Failed to find length byte in response from P3IO!\n");
        free(realinbuf);
        return 0;
    }

    /* Grab the real length of the packet */
    unsigned int realLen = realinbuf[loc++];

    /* Skip past the sequence because we don't care */
    loc++;
    realLen--;

    for (unsigned int i = 0; i < inlen; i++)
    {
        if (i >= realLen)
        {
            /* We finished decoding the packet */
            break;
        }
        if (loc >= actual)
        {
            /* Not enough data */
            fprintf(stderr, "Ran out of data in response from P3IO!\n");
            free(realinbuf);
            return 0;
        }

        if (realinbuf[loc] == 0xFF)
        {
            /* Unescape this byte */
            inbuf[i] = ~realinbuf[loc + 1];
            loc += 2;
        }
        else
        {
            /* Output directly */
            inbuf[i] = realinbuf[loc];
            loc += 1;
        }
    }

    /* Return the real length read, even if it is longer than the buffer. We
       stop copying when we get to the buffer length, so this is for information
       to be passed to the caller. */
    free(realinbuf);
    return realLen;
}

bool IO::ExchangeEXTIO(unsigned int message)
{
    /* If the EXTIO isn't initialized, don't bother */
    if (!is_ready || extio == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    /* Construct packet, sign with CRC */
    unsigned char outbuf[4] = { (message & 0xFF) | 0x80, (message >> 8) & 0xFF, (message >> 16) & 0xFF, 0x0 };
    outbuf[3] = (outbuf[0] + outbuf[1] + outbuf[2]) & 0x7F;

    /* Send it */
    DWORD actual = 0;
    WriteFile(extio, outbuf, 4, &actual, 0);
    FlushFileBuffers(extio);
    if (actual != 4)
    {
        fprintf(stderr, "Failed to write to EXTIO!\n");
        return false;
    }

    unsigned char inbuf[1] = { 0x0 };
    ReadFile(extio, inbuf, 1, &actual, 0);
    if (actual != 1)
    {
        fprintf(stderr, "Got unexpected size %d back from EXTIO!\n", actual);
        return false;
    }
    if (inbuf[0] != 0x11)
    {
        fprintf(stderr, "Got malformed response from EXTIO!\n");
        return false;
    }

    return true;
}

unsigned int IO::GetButtonsHeld()
{
    /* This is an IOCTL instead of a normal write/read operation.
       Presumably this is for latency guarantees over USB. */
    if (!is_ready)
    {
        return 0;
    }

    /* Poll the device using an IOCTL */
    unsigned char realoutbuf[16] = { 0x0 };
    DWORD actual = 0;
    if (!DeviceIoControl(p3io, 0x222068, 0, 0, realoutbuf, 16, &actual, 0))
    {
        fprintf(stderr, "Failed to poll for buttons!\n");
        return 0;
    }

    /* I don't know why we could get 16 bytes back, but we seem to always get 12 */
    if (actual != 12)
    {
        fprintf(stderr, "Got unexpected size %d back from button poll!\n", actual);
        return 0;
    }

    /* Map the return to our actual buttons */
    unsigned int newbuttons = 0;

    /* JAMMA reports active low, so lets swap relevant bytes */
    unsigned char buttonbuf[4] = {
        (~realoutbuf[1]) & 0xFF,
        (~realoutbuf[2]) & 0xFF,
        (~realoutbuf[3]) & 0xFF,
        (~realoutbuf[4]) & 0xFF
    };

    /* 1P Pad inputs */
    if (buttonbuf[0] & 0x02)
    {
        newbuttons |= BUTTON_1P_UP;
    }
    if (buttonbuf[0] & 0x04)
    {
        newbuttons |= BUTTON_1P_DOWN;
    }
    if (buttonbuf[0] & 0x08)
    {
        newbuttons |= BUTTON_1P_LEFT;
    }
    if (buttonbuf[0] & 0x10)
    {
        newbuttons |= BUTTON_1P_RIGHT;
    }

    /* 2P Pad inputs */
    if (buttonbuf[1] & 0x02)
    {
        newbuttons |= BUTTON_2P_UP;
    }
    if (buttonbuf[1] & 0x04)
    {
        newbuttons |= BUTTON_2P_DOWN;
    }
    if (buttonbuf[1] & 0x08)
    {
        newbuttons |= BUTTON_2P_LEFT;
    }
    if (buttonbuf[1] & 0x10)
    {
        newbuttons |= BUTTON_2P_RIGHT;
    }

    /* 1P Menu inputs */
    if (buttonbuf[3] & 0x01)
    {
        newbuttons |= BUTTON_1P_MENUUP;
    }
    if (buttonbuf[3] & 0x02)
    {
        newbuttons |= BUTTON_1P_MENUDOWN;
    }
    if (buttonbuf[0] & 0x40)
    {
        newbuttons |= BUTTON_1P_MENULEFT;
    }
    if (buttonbuf[0] & 0x80)
    {
        newbuttons |= BUTTON_1P_MENURIGHT;
    }

    /* 2P Menu inputs */
    if (buttonbuf[3] & 0x04)
    {
        newbuttons |= BUTTON_2P_MENUUP;
    }
    if (buttonbuf[3] & 0x08)
    {
        newbuttons |= BUTTON_2P_MENUDOWN;
    }
    if (buttonbuf[1] & 0x40)
    {
        newbuttons |= BUTTON_2P_MENULEFT;
    }
    if (buttonbuf[1] & 0x80)
    {
        newbuttons |= BUTTON_2P_MENURIGHT;
    }

    /* Start inputs */
    if (buttonbuf[0] & 0x01)
    {
        newbuttons |= BUTTON_1P_START;
    }
    if (buttonbuf[1] & 0x01)
    {
        newbuttons |= BUTTON_2P_START;
    }

    /* Cabinet buttons */
    if (buttonbuf[2] & 0x20)
    {
        newbuttons |= BUTTON_COIN;
    }
    if (buttonbuf[2] & 0x40)
    {
        newbuttons |= BUTTON_SERVICE;
    }
    if (buttonbuf[2] & 0x10)
    {
        newbuttons |= BUTTON_TEST;
    }

    return newbuttons;
}

void IO::Tick()
{
    if (!is_ready) { return; }

    // Remember last buttons so we can calculate newly
    // pressed buttons.
    lastButtons = buttons;

    // Poll the JAMMA edge to get current held buttons
    buttons = GetButtonsHeld();
}

unsigned int IO::ButtonsPressed()
{
    // Return only buttons pressed in the last Tick() operation.
    return buttons & (~lastButtons);
}

bool IO::ButtonPressed(unsigned int button)
{
    // Return whether the current button mask is pressed.
    return (ButtonsPressed() & button) != 0;
}

unsigned int IO::ButtonsHeld()
{
    // Return all buttons currently held down.
    return buttons;
}

bool IO::ButtonHeld(unsigned int button)
{
    // Return whether the current button mask is held.
    return (ButtonsHeld() & button) != 0;
}

void IO::LightOn(unsigned int light)
{
    SetLights((lastcablights | lastpadlights) | light);
}

void IO::LightOff(unsigned int light)
{
    SetLights((lastcablights | lastpadlights) & (~light));
}
