/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: window_x11.c
**
**  Description:
**      Simulate CDC 6612 or CC545 console display on X11R6.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

/*
**  -------------
**  Include Files
**  -------------
*/
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define ListSize           5000
#define FrameTime          100000
#define FramesPerSecond    (1000000 / FrameTime)

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct dispList
    {
    u16 xPos;                       /* horizontal position */
    u16 yPos;                       /* vertical position */
    u8  fontSize;                   /* size of font */
    u8  ch;                         /* character to be displayed */
    } DispList;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
void *windowThread(void *param);

/*
**  ----------------
**  Public Variables
**  ----------------
*/

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static volatile bool   displayActive = FALSE;
static u8              currentFont;
static i16             currentX;
static i16             currentY;
static DispList        display[ListSize];
static pthread_t       displayThread;
static u32             listEnd;
static Font            hSmallFont;
static Font            hMediumFont;
static Font            hLargeFont;
static int             width;
static int             height;
static pthread_mutex_t mutexDisplay;
static Display         *disp;
static Window          window;
static u8              *lpClipToKeyboard    = NULL;
static u8              *lpClipToKeyboardPtr = NULL;
static u8              clipToKeyboardDelay  = 0;

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Create POSIX thread which will deal with all X11
**                  functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowInit(void)
    {
    int            rc;
    pthread_attr_t attr;

    /*
    **  Create display list pool.
    */
    listEnd = 0;

    /*
    **  Create a mutex to synchronise access to display list.
    */
    pthread_mutex_init(&mutexDisplay, NULL);

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc            = pthread_create(&displayThread, &attr, windowThread, NULL);
    displayActive = TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set font size.
**                  functions.
**
**  Parameters:     Name        Description.
**                  size        font size in points.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowSetFont(u8 font)
    {
    currentFont = font;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set X coordinate.
**
**  Parameters:     Name        Description.
**                  x           horinzontal coordinate (0 - 0777)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowSetX(u16 x)
    {
    currentX = x;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set Y coordinate.
**
**  Parameters:     Name        Description.
**                  y           horinzontal coordinate (0 - 0777)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowSetY(u16 y)
    {
    currentY = 0777 - y;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue characters.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowQueue(u8 ch)
    {
    DispList *elem;

    if ((listEnd >= ListSize)
        || (currentX == -1)
        || (currentY == -1))
        {
        return;
        }

    /*
    **  Protect display list.
    */
    pthread_mutex_lock(&mutexDisplay);

    if (ch != 0)
        {
        elem           = display + listEnd++;
        elem->ch       = ch;
        elem->fontSize = currentFont;
        elem->xPos     = currentX;
        elem->yPos     = currentY;
        }

    currentX += currentFont;

    /*
    **  Release display list.
    */
    pthread_mutex_unlock(&mutexDisplay);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate console window.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowTerminate(void)
    {
    if (displayActive)
        {
        displayActive = FALSE;
        pthread_join(displayThread, NULL);
        }
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Windows thread.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void *windowThread(void *param)
    {
    XWindowAttributes a;
    XColor            b;
    unsigned long     bg;
    XColor            c;
    DispList          *curr;
    int               depth;
    DispList          *end;
    XEvent            event;
    unsigned long     fg;
    GC                gc;
    bool              isMeta;
    KeySym            key;
    int               len;
    u8                oldFont = 0;
    Pixmap            pixmap;
    static int        refreshCount = 0;
    Atom              retAtom;
    int               retFormat;
    unsigned long     retLength;
    unsigned long     retRemaining;
    int               retStatus;
    int               screen;
    char              str[2] = " ";
    Atom              targetProperty;
    char              text[30];
    int               usageDisplayCount = 0;
    char              windowTitle[132];
    Atom              wmDeleteWindow;
    XWMHints          wmhints;
    char              xFontName[132];

    /*
    **  Open the X11 display.
    */
    disp = XOpenDisplay(0);
    if (disp == (Display *)NULL)
        {
        logDtError(LogErrorLocation, "Could not open display\n");
        exit(1);
        }

    screen = DefaultScreen(disp);

    /*
    **  Create a window using the following hints.
    */
    width  = 1100;
    height = 750;

    bg = BlackPixel(disp, screen);
    fg = WhitePixel(disp, screen);

    window = XCreateSimpleWindow(disp, DefaultRootWindow(disp),
                                 10, 10, width, height, 5, fg, bg);

    /*
    **  Create a pixmap for background image generation.
    */
    depth  = DefaultDepth(disp, screen);
    pixmap = XCreatePixmap(disp, window, width, height, depth);

    /*
    **  Set window and icon titles.
    */
    windowTitle[0] = '\0';
    strcat(windowTitle, displayName);
    strcat(windowTitle, " - " DtCyberVersion);
    strcat(windowTitle, " - " DtCyberBuildDate);

    XSetStandardProperties(disp, window, windowTitle,
                           DtCyberVersion, None, NULL, 0, NULL);

    /*
    **  Create the graphics contexts for window and pixmap.
    */
    gc = XCreateGC(disp, window, 0, 0);

    /*
    **  We don't want to get Expose events, otherwise every XCopyArea will generate one,
    **  and the event queue will fill up. This application will discard them anyway, but
    **  it is better not to generate them in the first place.
    */
    XSetGraphicsExposures(disp, gc, FALSE);

    /*
    **  Load three Cyber fonts.
    */
    sprintf(xFontName, "-*-%s-medium-*-*-*-%ld-*-*-*-*-*-*-*", fontName, fontSmall);
    hSmallFont = XLoadFont(disp, xFontName);
    sprintf(xFontName, "-*-%s-medium-*-*-*-%ld-*-*-*-*-*-*-*", fontName, fontMedium);
    hMediumFont = XLoadFont(disp, xFontName);
    sprintf(xFontName, "-*-%s-medium-*-*-*-%ld-*-*-*-*-*-*-*", fontName, fontLarge);
    hLargeFont = XLoadFont(disp, xFontName);

    /*
    **  Setup fore- and back-ground colors.
    */
    XGetWindowAttributes(disp, window, &a);
    XAllocNamedColor(disp, a.colormap, colorFG, &b, &c);
    fg = b.pixel;
    XAllocNamedColor(disp, a.colormap, colorBG, &b, &c);
    bg = b.pixel;

    XSetBackground(disp, gc, bg);
    XSetForeground(disp, gc, fg);

    /*
    **  Initialise input.
    */
    wmhints.flags = InputHint;
    wmhints.input = True;
    XSetWMHints(disp, window, &wmhints);
    XSelectInput(disp, window, KeyPressMask | KeyReleaseMask | StructureNotifyMask);

    /*
    **  We like to be on top.
    */
    XMapRaised(disp, window);

    /*
    **  Create atom for paste operations,
    */
    targetProperty = XInternAtom(disp, "DtCYBER", False);

    /*
    **  Create atom for delete message and set window manager.
    */
    wmDeleteWindow = XInternAtom(disp, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(disp, window, &wmDeleteWindow, 1);

    /*
    **  Window thread loop.
    */
    isMeta = FALSE;

    while (displayActive)
        {
        /*
        **  Process paste buffer one character a time.
        */
        if (lpClipToKeyboardPtr != NULL)
            {
            if (clipToKeyboardDelay != 0)
                {
                /*
                **  Delay after CR.
                */
                clipToKeyboardDelay -= 1;
                }
            else
                {
                ppKeyIn = *lpClipToKeyboardPtr++;
                if (ppKeyIn == 0)
                    {
                    /*
                    **  All paste data has been processed - clean up.
                    */
                    XFree(lpClipToKeyboard);
                    lpClipToKeyboard    = NULL;
                    lpClipToKeyboardPtr = NULL;
                    }
                else if (ppKeyIn == '\n')
                    {
                    /*
                    **  Substitute to a CR to be able to handle DOS/Windows or UNIX style
                    **  line terminators.
                    */
                    ppKeyIn = '\r';

                    /*
                    **  Short delay to allow PP program to process the line. This may
                    **  require customisation.
                    */
                    clipToKeyboardDelay = 30;
                    }
                else if (ppKeyIn == '\r')
                    {
                    /*
                    **  Ignore CR.
                    */
                    ppKeyIn = 0;
                    }
                }
            }

        /*
        **  Process any X11 events.
        */
        while (XEventsQueued(disp, QueuedAfterReading))
            {
            XNextEvent(disp, &event);

            switch (event.type)
                {
            case ClientMessage:
                if (event.xclient.data.l[0] == wmDeleteWindow)
                    {
                    /*
                    **  Initiate display of usage note because user attempts to close the window.
                    */
                    usageDisplayCount = 5 * FramesPerSecond;
                    }

                break;

            case MappingNotify:
                XRefreshKeyboardMapping((XMappingEvent *)&event);
                break;

            case ConfigureNotify:
                if ((event.xconfigure.width > width) || (event.xconfigure.height > height))
                    {
                    /*
                    **  Reallocate pixmap only if it has grown.
                    */
                    width  = event.xconfigure.width;
                    height = event.xconfigure.height;
                    XFreePixmap(disp, pixmap);
                    pixmap = XCreatePixmap(disp, window, width, height, depth);
                    }

                XFillRectangle(disp, pixmap, gc, 0, 0, width, height);
                break;

            case KeyPress:
                len = XLookupString((XKeyEvent *)&event, text, 10, &key, 0);
                if (len < 1)
                    {
                    if (key == XK_Meta_L)
                        {
                        isMeta = TRUE;
                        }
                    }
                else if (len == 1)
                    {
                    if (isMeta == FALSE)
                        {
                        ppKeyIn = text[0];
                        sleepMsec(5);
                        }
                    else
                        {
                        switch (text[0])
                            {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                            traceMask ^= (1 << (text[0] - '0'));
                            break;

                        case 'c':
                            traceMask ^= (1 << 14);
                            break;

                        case 'e':
                            traceMask ^= (1 << 15);
                            break;

                        case 'x':
                            if (traceMask == 0)
                                {
                                traceMask = ~0;
                                }
                            else
                                {
                                traceMask = 0;
                                }
                            break;

                        case 'p':
                            if (lpClipToKeyboardPtr != NULL)
                                {
                                /*
                                **  Ignore paste request when a previous one is still executing.
                                */
                                break;
                                }

                            if (targetProperty == None)
                                {
                                /*
                                **  The paste operation atom has not been created. This is bad, but
                                **  not fatal, so we silently ignore paste requests.
                                */
                                break;
                                }

                            /*
                            **  Request the server to send an event to the present owner of the selection,
                            **  asking the owner to convert the data in the selection to the required type.
                            */
                            XConvertSelection(disp, XA_PRIMARY, XA_STRING, targetProperty, window, event.xbutton.time);
                            break;
                            }
                        ppKeyIn = 0;
                        }
                    }
                break;

            case KeyRelease:
                len = XLookupString((XKeyEvent *)&event, text, 10, &key, 0);
                if ((len < 1) && (key == XK_Meta_L))
                    {
                    isMeta = FALSE;
                    }
                break;

            case SelectionNotify:
                /*
                **  The present owner of the selection has replied.
                */
                if (event.xselection.property != targetProperty)
                    {
                    /*
                    **  The current selection is not a string, so we ignore it.
                    */
                    break;
                    }

                /*
                **  Fetch up to 1 kb from the selection.
                */
                retStatus = XGetWindowProperty(disp, window, event.xselection.property,
                                               0L, 1024, False, AnyPropertyType, &retAtom, &retFormat,
                                               &retLength, &retRemaining, &lpClipToKeyboard);

                if (retStatus == Success)
                    {
                    lpClipToKeyboardPtr = lpClipToKeyboard;
                    }
                else
                    {
                    lpClipToKeyboard    = NULL;
                    lpClipToKeyboardPtr = NULL;
                    }

                break;
                }
            }

        XSetForeground(disp, gc, fg);

        XSetFont(disp, gc, hSmallFont);
        oldFont = FontSmall;

#if CcCycleTime
            {
            extern double cycleTime;
            char          buf[80];

            sprintf(buf, "Cycle time: %.3f", cycleTime);
            XDrawString(disp, pixmap, gc, 0, 10, buf, strlen(buf));
            }
#endif

#if CcDebug == 1
            {
            char buf[160];

            /*
            **  Display P registers of PPUs and CPUs and current trace mask.
            */
            sprintf(buf, "Refresh: %-10d  PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o   CPU P-reg: %06o",
                    refreshCount++,
                    ppu[0].regP, ppu[1].regP, ppu[2].regP, ppu[3].regP, ppu[4].regP,
                    ppu[5].regP, ppu[6].regP, ppu[7].regP, ppu[8].regP, ppu[9].regP,
                    cpus[0].regP);
            if (cpuCount > 1)
                {
                sprintf(buf + strlen(buf), " %06o", cpus[1].regP);
                }

            sprintf(buf + strlen(buf), "   Trace: %c%c%c%c%c%c%c%c%c%c%c%c",
                    (traceMask >> 0) & 1 ? '0' : '_',
                    (traceMask >> 1) & 1 ? '1' : '_',
                    (traceMask >> 2) & 1 ? '2' : '_',
                    (traceMask >> 3) & 1 ? '3' : '_',
                    (traceMask >> 4) & 1 ? '4' : '_',
                    (traceMask >> 5) & 1 ? '5' : '_',
                    (traceMask >> 6) & 1 ? '6' : '_',
                    (traceMask >> 7) & 1 ? '7' : '_',
                    (traceMask >> 8) & 1 ? '8' : '_',
                    (traceMask >> 9) & 1 ? '9' : '_',
                    (traceMask >> 14) & 1 ? 'C' : '_',
                    (traceMask >> 15) & 1 ? 'E' : '_');

            XDrawString(disp, pixmap, gc, 0, 10, buf, strlen(buf));
            }
#endif

        if (opPaused)
            {
            /*
            **  Display pause message.
            */
            static char opMessage[] = "Emulation paused";
            XSetFont(disp, gc, hLargeFont);
            oldFont = FontLarge;
            XDrawString(disp, pixmap, gc, 20, 256, opMessage, strlen(opMessage));
            }
        else if (consoleIsRemoteActive())
            {
            /*
            **  Display indication that rmeote console is active.
            */
            static char opMessage[] = "Remote console active";
            XSetFont(disp, gc, hLargeFont);
            oldFont = FontLarge;
            XDrawString(disp, pixmap, gc, 20, 256, opMessage, strlen(opMessage));
            }

        /*
        **  Protect display list.
        */
        pthread_mutex_lock(&mutexDisplay);

        if (usageDisplayCount != 0)
            {
            /*
            **  Display usage note when user attempts to close window.
            */
            static char usageMessage1[] = "Please don't just close the window, but instead first cleanly halt the operating system and";
            static char usageMessage2[] = "then use the 'shutdown' command in the operator interface to terminate the emulation.";
            XSetFont(disp, gc, hMediumFont);
            oldFont = FontMedium;
            XDrawString(disp, pixmap, gc, 20, 256, usageMessage1, strlen(usageMessage1));
            XDrawString(disp, pixmap, gc, 20, 275, usageMessage2, strlen(usageMessage2));
            listEnd            = 0;
            usageDisplayCount -= 1;
            }

        /*
        **  Draw display list in pixmap.
        */
        curr = display;
        end  = display + listEnd;

        for (curr = display; curr < end; curr++)
            {
            /*
            **  Setup new font if necessary.
            */
            if (oldFont != curr->fontSize)
                {
                oldFont = curr->fontSize;

                switch (oldFont)
                    {
                case FontSmall:
                    XSetFont(disp, gc, hSmallFont);
                    break;

                case FontMedium:
                    XSetFont(disp, gc, hMediumFont);
                    break;

                case FontLarge:
                    XSetFont(disp, gc, hLargeFont);
                    break;
                    }
                }

            /*
            **  Draw dot or character.
            */
            if (curr->fontSize == FontDot)
                {
                XDrawPoint(disp, pixmap, gc, curr->xPos, (curr->yPos * 14) / 10 + 20);
                }
            else
                {
                str[0] = curr->ch;
                XDrawString(disp, pixmap, gc, curr->xPos, (curr->yPos * 14) / 10 + 20, str, 1);
                }
            }

        listEnd  = 0;
        currentX = -1;
        currentY = -1;

        /*
        **  Release display list.
        */
        pthread_mutex_unlock(&mutexDisplay);

        /*
        **  Update display from pixmap.
        */
        XCopyArea(disp, pixmap, window, gc, 0, 0, width, height, 0, 0);

        /*
        **  Erase pixmap for next round.
        */
        XSetForeground(disp, gc, bg);
        XFillRectangle(disp, pixmap, gc, 0, 0, width, height);

        /*
        **  Make sure the updates make it to the X11 server.
        */
        XSync(disp, 0);

        /*
        **  Give other threads a chance to run. This may require customisation.
        */
        sleepUsec(FrameTime);
        }

    XSync(disp, 0);
    XFreeGC(disp, gc);
    XFreePixmap(disp, pixmap);
    XDestroyWindow(disp, window);
    XCloseDisplay(disp);
    pthread_mutex_destroy(&mutexDisplay);
    pthread_exit(NULL);
    }

/*---------------------------  End Of File  ------------------------------*/
