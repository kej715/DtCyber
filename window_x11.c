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
#include <X11/Xft/Xft.h>
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
static void windowActivateFont(u8 fontSize);
static void windowDrawString(int x, int y, char *s, int len);
static void *windowThread(void *param);

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
static unsigned long   bg;
static u8              *clipToKeyboard     = NULL;
static u8              *clipToKeyboardPtr  = NULL;
static u8              clipToKeyboardDelay = 0;
static u8              currentFont;
static i16             currentX;
static i16             currentY;
static int             depth;
static Display         *disp;
static DispList        display[ListSize];
static volatile bool   displayActive = FALSE;
static pthread_t       displayThread;
static unsigned long   fg;
static GC              gc;
static int             height;
static u32             listEnd;
static pthread_mutex_t mutexDisplay;
static Pixmap          pixmap;
static int             screen;
static Atom            targetProperty;
static int             width;
static Window          window;
static Atom            wmDeleteWindow;
static int             yFactor;
static int             yIncrement;

//
//  Variables related to rendering standard fonts
//
static Font            stdSmallFont;
static Font            stdMediumFont;
static Font            stdLargeFont;

//
//  Variables related to rendering XFT (TrueType) fonts
//
static XftDraw         *xftDraw;
static XftFont         *xftFont;
static XftFont         *xftSmallFont;
static XftFont         *xftMediumFont;
static XftFont         *xftLargeFont;
static XftColor        xftColor;


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
    XWindowAttributes a;
    pthread_attr_t    attr;
    XColor            b;
    XColor            c;
    int               rc;
    char              windowTitle[132];
    XWMHints          wmHints;
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
    **  Setup foreground and background colors.
    */
    XGetWindowAttributes(disp, window, &a);
    XAllocNamedColor(disp, a.colormap, colorFG, &b, &c);
    fg = b.pixel;
    XAllocNamedColor(disp, a.colormap, colorBG, &b, &c);
    bg = b.pixel;

    XSetBackground(disp, gc, bg);
    XSetForeground(disp, gc, fg);

    /*
    **  Load three Cyber fonts.
    */
    if (fontIsTrueType)
        {
        yFactor    = 12;
        yIncrement = 16;
        xftDraw = XftDrawCreate(disp, pixmap, DefaultVisual(disp, screen), a.colormap);
        sprintf(xFontName, "%s-%ld", fontName, fontSmall);
        xftSmallFont = XftFontOpenName(disp, screen, xFontName);
        if (xftSmallFont == 0)
            {
            logDtError(LogErrorLocation, "Could not open font %s\n", fontName);
            exit(1);
            }
        sprintf(xFontName, "%s-%ld", fontName, fontMedium);
        xftMediumFont = XftFontOpenName(disp, screen, xFontName);
        if (xftMediumFont == 0)
            {
            logDtError(LogErrorLocation, "Could not open font %s\n", fontName);
            exit(1);
            }
        sprintf(xFontName, "%s-%ld", fontName, fontLarge);
        xftLargeFont = XftFontOpenName(disp, screen, xFontName);
        if (xftLargeFont == 0)
            {
            logDtError(LogErrorLocation, "Could not open font %s\n", fontName);
            exit(1);
            }
        if (XftColorAllocName(disp, DefaultVisual(disp, screen), a.colormap, colorFG, &xftColor) == FALSE)
            {
            logDtError(LogErrorLocation, "Could not allocate color '%s'\n", colorFG);
            exit(1);
            }
        }
    else
        {
        yFactor    = 14;
        yIncrement = 20;
        sprintf(xFontName, "-*-%s-medium-*-*-*-%ld-*-*-*-*-*-*-*", fontName, fontSmall);
        stdSmallFont = XLoadFont(disp, xFontName);
        sprintf(xFontName, "-*-%s-medium-*-*-*-%ld-*-*-*-*-*-*-*", fontName, fontMedium);
        stdMediumFont = XLoadFont(disp, xFontName);
        sprintf(xFontName, "-*-%s-medium-*-*-*-%ld-*-*-*-*-*-*-*", fontName, fontLarge);
        stdLargeFont = XLoadFont(disp, xFontName);
        }

    /*
    **  Initialise input.
    */
    wmHints.flags = InputHint;
    wmHints.input = True;
    XSetWMHints(disp, window, &wmHints);
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
**  Purpose:        Activate a specified size of the configured font.
**
**  Parameters:     Name        Description.
**                  fontSize    the size (one of FontSmall, FontMedium, FontLarge)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void windowActivateFont(u8 fontSize)
    {
    Font stdFont;

    if (fontIsTrueType)
        {
        switch (fontSize)
            {
        default:
        case FontSmall:
            xftFont = xftSmallFont;
            break;
        case FontMedium:
            xftFont = xftMediumFont;
            break;
        case FontLarge:
            xftFont = xftLargeFont;
            break;
            }
        }
    else
        {
        switch (fontSize)
            {
        default:
        case FontSmall:
            stdFont = stdSmallFont;
            break;

        case FontMedium:
            stdFont = stdMediumFont;
            break;

        case FontLarge:
            stdFont = stdLargeFont;
            break;
            }
        XSetFont(disp, gc, stdFont);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Draw a string at a specified screen coordinate.
**
**  Parameters:     Name        Description.
**                  x           the X coordinate
**                  y           the Y coordinate
**                  s           the string to draw
**                  len         the length of the string
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void windowDrawString(int x, int y, char *s, int len)
    {
    if (fontIsTrueType)
        {
        XftDrawStringUtf8(xftDraw, &xftColor, xftFont, x, y, (FcChar8 *)s, len);
        }
    else
        {
        XDrawString(disp, pixmap, gc, x, y, s, len);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Window thread.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void *windowThread(void *param)
    {
    DispList          *curr;
    DispList          *end;
    XEvent            event;
    bool              isMeta;
    KeySym            key;
    int               len;
    u8                oldFont = 0;
    static int        refreshCount = 0;
    Atom              retAtom;
    int               retFormat;
    unsigned long     retLength;
    unsigned long     retRemaining;
    int               retStatus;
    char              str[2] = " ";
    char              text[30];
    int               usageDisplayCount = 0;

    /*
    **  Window thread loop.
    */
    isMeta = FALSE;

    while (displayActive)
        {
        /*
        **  Process paste buffer one character a time.
        */
        if (clipToKeyboardPtr != NULL)
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
                ppKeyIn = *clipToKeyboardPtr++;
                if (ppKeyIn == 0)
                    {
                    /*
                    **  All paste data has been processed - clean up.
                    */
                    XFree(clipToKeyboard);
                    clipToKeyboard    = NULL;
                    clipToKeyboardPtr = NULL;
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

                        case 'b':
                            traceMask ^= TraceBlockOp;
                            break;

                        case 'c':
                            traceMask ^= TraceCpu170;
                            break;

                        case 'e':
                            traceMask ^= TraceExchange;
                            break;

                        case 'f':
                            traceMask ^= TraceCallFrame;
                            break;

                        case 'v':
                            traceMask ^= TraceCpu180;
                            break;

                        case 'w':
                            traceMask ^= TraceCpu180|TraceExchange|TraceBlockOp|TraceCallFrame;
                            break;

                        case 'x':
                            traceMask = 0;
                            break;

                        case 'p':
                            if (clipToKeyboardPtr != NULL)
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
                                               &retLength, &retRemaining, &clipToKeyboard);

                if (retStatus == Success)
                    {
                    clipToKeyboardPtr = clipToKeyboard;
                    }
                else
                    {
                    clipToKeyboard    = NULL;
                    clipToKeyboardPtr = NULL;
                    }

                break;
                }
            }

        XSetForeground(disp, gc, fg);

        windowActivateFont(FontSmall);
        oldFont = FontSmall;

#if CcCycleTime
            {
            extern double cycleTime;
            char          buf[80];

            sprintf(buf, "Cycle time: %.3f", cycleTime);
            windowDrawString(0, 10, buf, strlen(buf));
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
                    cpus170[0].regP);
            if (cpuCount > 1)
                {
                sprintf(buf + strlen(buf), " %06o", cpus170[1].regP);
                }

            sprintf(buf + strlen(buf), "   Trace: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
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
                    (traceMask & TraceCpu170) != 0 ? 'C' : '_',
                    (traceMask & TraceCpu180) != 0 ? 'V' : '_',
                    (traceMask & TraceExchange) != 0 ? 'E' : '_',
                    (traceMask & TraceBlockOp) != 0 ? 'B' : '_',
                    (traceMask & TraceCallFrame) != 0 ? 'F' : '_');

            windowDrawString(0, 10, buf, strlen(buf));
            }
#endif

        if (opPaused)
            {
            /*
            **  Display pause message.
            */
            static char opMessage[] = "Emulation paused";
            windowActivateFont(FontLarge);
            oldFont = FontLarge;
            windowDrawString(20, 256, opMessage, strlen(opMessage));
            }
        else if (consoleIsRemoteActive())
            {
            /*
            **  Display indication that rmeote console is active.
            */
            static char opMessage[] = "Remote console active";
            windowActivateFont(FontLarge);
            oldFont = FontLarge;
            windowDrawString(20, 256, opMessage, strlen(opMessage));
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
            windowActivateFont(FontMedium);
            oldFont = FontMedium;
            windowDrawString(20, 256, usageMessage1, strlen(usageMessage1));
            windowDrawString(20, 275, usageMessage2, strlen(usageMessage2));
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
                windowActivateFont(oldFont);
                }

            /*
            **  Draw dot or character.
            */
            if (curr->fontSize == FontDot)
                {
                XDrawPoint(disp, pixmap, gc, curr->xPos, (curr->yPos * yFactor) / 10 + yIncrement);
                }
            else
                {
                str[0] = curr->ch;
                windowDrawString(curr->xPos, (curr->yPos * yFactor) / 10 + yIncrement, str, 1);
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

    if (fontIsTrueType)
        {
        XftFontClose(disp, xftSmallFont);
        XftFontClose(disp, xftMediumFont);
        XftFontClose(disp, xftLargeFont);
        XftColorFree(disp, DefaultVisual(disp, screen), DefaultColormap(disp, screen), &xftColor);
        XftDrawDestroy(xftDraw);
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
