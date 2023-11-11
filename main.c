#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <coreinit/cache.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <vpad/input.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_console.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

/* Source: https://github.com/yawut/ProgrammingOnTheU */
/* Additional example: https://github.com/devkitPro/wut/blob/master/samples/make/helloworld/source/main.c */
int main(void)
{
    /* Init WHB's ProcUI wrapper. This will manage all the little Cafe OS bits and
    pieces for us - home menu overlay, power saving features, etc. */
    WHBProcInit();

    /* Init logging. We log to Cafe's internal logger (shows up in Decaf and some
    crash logs), over UDP for with udplogserver, and to the console. */
    WHBLogCafeInit();
    WHBLogUdpInit();
    WHBLogConsoleInit();

    /* WHBLogPrint and WHBLogPrintf add new line characters for you */
    WHBLogPrint("Hello, World! Logging started.");
    WHBLogPrint("Starting OSScreen in 2 seconds.");
    WHBLogConsoleDraw();
    OSSleepTicks(OSMillisecondsToTicks(2000));

    /* Init OSScreen. This is the really simple graphics API we'll be using to
    draw some text! Also exit out of WHB console logging. */
    WHBLogConsoleFree();
    OSScreenInit();

    /* OSScreen needs buffers for each display - get the size of those now.
    "DRC" is Nintendo's acronym for the GamePad. */
    size_t tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    size_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    WHBLogPrintf("Will allocate 0x%X bytes for the TV, "
                 "and 0x%X bytes for the GamePad.",
                 tvBufferSize, drcBufferSize);

    /* Try to allocate an area for the buffers. According to OSScreenSetBufferEx's
    documentation, these need to be 0x100 aligned. */
    void *tvBuffer = memalign(0x100, tvBufferSize);
    void *drcBuffer = memalign(0x100, drcBufferSize);

    /* Make sure the allocation actually succeeded! */
    if (!tvBuffer || !drcBuffer)
    {
        WHBLogPrint("Out of memory!");

        /* It's vital to free everything - under certain circumstances, your memory
        allocations can stay allocated even after you quit. */
        if (tvBuffer)
            free(tvBuffer);
        if (drcBuffer)
            free(drcBuffer);

        /* Deinit everything */
        OSScreenShutdown();

        WHBLogPrint("Quitting.");
        WHBLogCafeDeinit();
        WHBLogUdpDeinit();

        WHBProcShutdown();

        /* Your exit code doesn't really matter, though that may be changed in
        future. Don't use -3, that's reserved for HBL. */
        return 1;
    }

    /* Buffers are all good, set them */
    OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);

    /* Finally, enable OSScreen for each display! */
    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);

    /* Create the GamePad button press variables. */
    VPADStatus status;
    VPADReadError error;
    VPADTouchData touchData;
    bool vpad_fatal = false;

    /* WHBProcIsRunning will return false if the OS asks us to quit, so it's a
    good candidate for a loop */
    while (WHBProcIsRunning())
    {
        /* Clear each buffer - the 0x... is an RGBX colour */
        OSScreenClearBufferEx(SCREEN_TV, 0x0000A000);
        OSScreenClearBufferEx(SCREEN_DRC, 0x00A00000);

        /* Print some text. Coordinates are (columns, rows). */
        OSScreenPutFontEx(SCREEN_TV, 0, 0, "Hello, world! This is the TV.");
        OSScreenPutFontEx(SCREEN_TV, 0, 1,
                          "If you can read this, the homebrew app is working.");
        OSScreenPutFontEx(SCREEN_TV, 0, 3,
                          "Press A, B, X, or Y on the GamePad to test input. "
                          "Most output will be on the GamePad screen.");

        OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Hello, world! This is the GamePad.");
        OSScreenPutFontEx(SCREEN_DRC, 0, 1,
                          "If you can read this, the homebrew app is working.");

        /* Read button, touch and sensor data from the Gamepad */
        VPADRead(VPAD_CHAN_0, &status, 1, &error);
        /* Check for any errors */
        switch (error)
        {
        case VPAD_READ_SUCCESS:
        {
            /* Everything worked, awesome! */
            OSScreenPutFontEx(SCREEN_DRC, 0, 10,
                              "Successfully read the state of the GamePad!");
            break;
        }
            /* No data available from the DRC yet - we're asking too often!
            This is really common, and nothing to worry about. */
        case VPAD_READ_NO_SAMPLES:
        {
            /* Just keep looping, we'll get data eventually */
            OSScreenPutFontEx(SCREEN_DRC, 0, 10, "Got no input this cycle.");
            continue;
        }
            /* Either our channel was bad, or the controller is. Since this app
            hard-codes channel 0, we can assume something's up with the
            controller - maybe it's missing or off? */
        case VPAD_READ_INVALID_CONTROLLER:
        {
            WHBLogPrint("GamePad disconnected!");
            OSScreenPutFontEx(SCREEN_TV, 0, 10, "GamePad disconnected!");
            /* Not much point testing buttons for a controller that's not
                actually there */
            vpad_fatal = true;
            break;
        }
            /* If you hit this, good job! As far as we know VPADReadError will
            always be one of the above. */
        default:
        {
            WHBLogPrintf("Unknown VPAD error! %08X", error);
            OSScreenPutFontEx(SCREEN_TV, 0, 10, "Unknown error! Check logs.");
            vpad_fatal = true;
            break;
        }
        }
        /* If there was some kind of fatal issue reading the VPAD, break out of
        the ProcUI loop and quit. */
        if (vpad_fatal)
            break;

        /* Check the buttons, and log appropriate messages */
        if (status.hold & VPAD_BUTTON_A)
        {
            WHBLogPrint("Pressed A this cycle.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 3, "Pressing A!");
        }
        else if (status.release & VPAD_BUTTON_A)
        {
            WHBLogPrint("Released A.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 3, "Released A!");
        }
        else
        {
            OSScreenPutFontEx(SCREEN_DRC, 0, 3, "Not pressing A!");
        }

        if (status.hold & VPAD_BUTTON_B)
        {
            WHBLogPrint("Pressed B this cycle.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 4, "Pressing B!");
        }
        else if (status.release & VPAD_BUTTON_B)
        {
            WHBLogPrint("Released B.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 4, "Released B!");
        }
        else
        {
            OSScreenPutFontEx(SCREEN_DRC, 0, 4, "Not pressing B!");
        }

        if (status.hold & VPAD_BUTTON_X)
        {
            WHBLogPrint("Pressed X this cycle.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 5, "Pressing X!");
        }
        else if (status.release & VPAD_BUTTON_X)
        {
            WHBLogPrint("Released X.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 5, "Released X!");
        }
        else
        {
            OSScreenPutFontEx(SCREEN_DRC, 0, 5, "Not pressing X!");
        }

        if (status.hold & VPAD_BUTTON_Y)
        {
            WHBLogPrint("Pressed Y this cycle.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 6, "Pressing Y!");
        }
        else if (status.release & VPAD_BUTTON_Y)
        {
            WHBLogPrint("Released Y.");
            OSScreenPutFontEx(SCREEN_DRC, 0, 6, "Released Y!");
        }
        else
        {
            OSScreenPutFontEx(SCREEN_DRC, 0, 6, "Not pressing Y!");
        }

        /* Check the touch position and write it to the GamePad.
        Additionally, draw a square at that location. */
        touchData = status.tpNormal;
        if (touchData.touched != 0)
        {
            char touchDataString[50];
            sprintf(touchDataString, "Touching the screen at x:%d and y:%d",
                    touchData.x, touchData.y);
            OSScreenPutFontEx(SCREEN_DRC, 0, 8, touchDataString);
            for (int x = -10; x < 10; x++)
            {
                for (int y = -10; y < 10; y++)
                {
                    /* The pixel's location needs to be mapped from the touch area
                    of 4000x4000 to the screen area of 854x480. */
                    OSScreenPutPixelEx(SCREEN_DRC, touchData.x / 4.684 + x,
                                       (4000 - touchData.y) / 8.333 + y, 0xF0000000);
                    OSScreenPutPixelEx(SCREEN_TV, touchData.x / 4.684 + x,
                                       (4000 - touchData.y) / 8.333 + y, 0xF0000000);
                }
            }
        }

        /* Flush all caches - read the tutorial, please! */
        DCFlushRange(tvBuffer, tvBufferSize);
        DCFlushRange(drcBuffer, drcBufferSize);

        /* Flip buffers - the text is now on screen! Flipping is kinda like
        committing your graphics changes. */
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);

        OSSleepTicks(OSMillisecondsToTicks(100));
    }

    /* Once we get here, ProcUI said we should quit. */
    WHBLogPrint("Got shutdown request!");

    /* Print a goodbye message and then wait before exiting. */
    /*
    OSScreenPutFontEx(SCREEN_TV, 0, 6, "Goodbye! Quitting now.");
    OSScreenPutFontEx(SCREEN_DRC, 0, 12, "Goodbye! Quitting now.");
    DCFlushRange(tvBuffer, tvBufferSize);
    DCFlushRange(drcBuffer, drcBufferSize);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    OSSleepTicks(OSMillisecondsToTicks(1000));
    */

    /* It's vital to free everything - under certain circumstances, your memory
    allocations can stay allocated even after you quit. */
    if (tvBuffer)
        free(tvBuffer);
    if (drcBuffer)
        free(drcBuffer);

    /* Deinit everything */
    /*OSScreenShutdown();*/

    WHBLogCafeDeinit();
    WHBLogUdpDeinit();

    WHBProcShutdown();

    /* Your exit code doesn't really matter, though that may be changed in
    future. Don't use -3, that's reserved for HBL. */
    return 0;
}
