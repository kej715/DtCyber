/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: window_win32.c
**
**  Description:
**      Simulate CDC 6612 or CC545 console display on MS Windows.
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include "resource.h"
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define ListSize    5000
// useful for more stable screen shots
// #define ListSize            10000

#define TIMER_ID    1

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

typedef enum displaymode
    {
    ModeLeft, ModeCenter, ModeRight
    } DisplayMode;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void windowThread(void);
ATOM windowRegisterClass(HINSTANCE hInstance);
static BOOL windowCreate(void);
static void windowClipboard(HWND hWnd);
static LRESULT CALLBACK windowProcedure(HWND, UINT, WPARAM, LPARAM);
void windowDisplay(HWND hWnd);

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
static u8          currentFont;
static i16         currentX      = -1;
static i16         currentY      = -1;
static bool        displayActive = FALSE;
static DispList    display[ListSize];
static u32         listEnd;
static HANDLE      hThread;
static HWND        hWnd;
static HFONT       hSmallFont            = 0;
static HFONT       hMediumFont           = 0;
static HFONT       hLargeFont            = 0;
static HPEN        hPen                  = 0;
static HINSTANCE   hInstance             = 0;
static char        *lpClipToKeyboard     = NULL;
static char        *lpClipToKeyboardPtr  = NULL;
static u8          clipToKeyboardDelay   = 0;
static DisplayMode displayMode           = ModeCenter;
static bool        displayModeNeedsErase = FALSE;
static BOOL        shifted               = FALSE;


/*--------------------------------------------------------------------------
**  Purpose:        Create WIN32 thread which will deal with all windows
**                  functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowInit(void)
    {
    DWORD dwThreadId;
    int   thPriority = 0;

    /*
    **  Create display list pool.
    */
    listEnd = 0;

    /*
    **  Get our instance
    */
    hInstance = GetModuleHandle(NULL);

    /*
    **  Create windowing thread.
    */
    hThread = CreateThread(
        NULL,                                       // no security attribute
        0,                                          // default stack size
        (LPTHREAD_START_ROUTINE)windowThread,
        (LPVOID)NULL,                               // thread parameter
        0,                                          // not suspended
        &dwThreadId);                               // returns thread ID

    if (hThread == NULL)
        {
        MessageBox(NULL, "Operator Window Thread Creation Failed.", "dtCyber/window_win32", MB_OK + MB_ICONERROR);
        exit(1);
        }

    thPriority = GetThreadPriority(hThread);
    if (!SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL))
        {
        if (MessageBox(NULL, "Could Not Set Thread Priority. Continue?", "dtCyber/window_win32",
                       MB_YESNO + MB_ICONQUESTION + MB_DEFBUTTON1) == MB_DEFBUTTON2)
            {
            exit(1);
            }
        }

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
**                  x           horizontal coordinate (0 - 0777)
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
**                  y           vertical coordinate (0 - 0777)
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

    if (ch != 0)
        {
        elem           = display + listEnd++;
        elem->ch       = ch;
        elem->fontSize = currentFont;
        elem->xPos     = currentX;
        elem->yPos     = currentY;
        }

    currentX += currentFont;
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
        SendMessage(hWnd, WM_DESTROY, 0, 0);
        WaitForSingleObject(hThread, INFINITE);
        displayActive = FALSE;
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
static void windowThread(void)
    {
    MSG msg;

    /*
    **  Register the window class.
    */
    windowRegisterClass(hInstance);

    /*
    **  Create the window.
    */
    if (!windowCreate())
        {
        MessageBox(NULL, "(window_win32) window creation failed", "Error", MB_OK);

        return;
        }

    /*
    **  Main message loop.
    */
    while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Register the window class.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
ATOM windowRegisterClass(HINSTANCE hInstance)
    {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;
    wcex.lpfnWndProc   = (WNDPROC)windowProcedure;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, (LPCTSTR)IDI_CONSOLE);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = (LPCSTR)IDC_CONSOLE;
    wcex.lpszClassName = "CONSOLE";
    wcex.hIconSm       = LoadIcon(hInstance, (LPCTSTR)IDI_SMALL);

    return RegisterClassEx(&wcex);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create the main window.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if successful, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static BOOL windowCreate(void)
    {
    char windowName[132];

    windowName[0] = '\0';
    strcat(windowName, displayName);
    strcat(windowName, " - " DtCyberVersion);
    strcat(windowName, " - " DtCyberBuildDate);

#if CcLargeWin32Screen == 1
    hWnd = CreateWindow(
        "CONSOLE",              // Registered class name
        windowName,             // window name
        WS_OVERLAPPEDWINDOW,    // window style
        CW_USEDEFAULT,          // horizontal position of window
        0,                      // vertical position of window
        widthPX,                // window width
        heightPX,               // window height
        NULL,                   // handle to parent or owner window
        NULL,                   // menu handle or child identifier
        0,                      // handle to application instance
        NULL);                  // window-creation data
#else
    hWnd = CreateWindow(
        "CONSOLE",                                                                    // Registered class name
        windowName,                                                                   // window name
        (WS_OVERLAPPEDWINDOW | WS_EX_COMPOSITED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN), // window style
        CW_USEDEFAULT,                                                                // horizontal position of window
        CW_USEDEFAULT,                                                                // vertical position of window
        widthPX,                                                                      // window width
        heightPX,                                                                     // window height
        NULL,                                                                         // handle to parent or owner window
        NULL,                                                                         // menu handle or child identifier
        0,                                                                            // handle to application instance
        NULL);                                                                        // window-creation data
#endif

    if (!hWnd)
        {
        return FALSE;
        }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    SetTimer(hWnd, TIMER_ID, timerRate, NULL);

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Copy clipboard data to keyboard buffer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void windowClipboard(HWND hWnd)
    {
    HANDLE hClipMemory;
    char   *lpClipMemory;

    if (!IsClipboardFormatAvailable(CF_TEXT)
        || !OpenClipboard(hWnd))
        {
        return;
        }

    hClipMemory = GetClipboardData(CF_TEXT);
    if (hClipMemory == NULL)
        {
        CloseClipboard();

        return;
        }

    lpClipToKeyboard = malloc(GlobalSize(hClipMemory));
    if (lpClipToKeyboard != NULL)
        {
        lpClipMemory = GlobalLock(hClipMemory);
        strcpy(lpClipToKeyboard, lpClipMemory);
        GlobalUnlock(hClipMemory);
        lpClipToKeyboardPtr = lpClipToKeyboard;
        }

    CloseClipboard();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process messages for the main window.
**
**  Parameters:     Name        Description.
**
**  Returns:        LRESULT
**
**------------------------------------------------------------------------*/
static LRESULT CALLBACK windowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
    int     wmId, wmEvent;
    LOGFONT lfTmp;
    RECT    rt;

    switch (message)
        {
    /*
    **  Process the application menu.
    */
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);

        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_ERASEBKGND:
        return (1);

        break;

    case WM_CREATE:
        hPen = CreatePen(PS_SOLID, 1, colorFG);
        if (!hPen)
            {
            MessageBox(GetFocus(),
                       "Unable to get foreground pen",
                       "(window_win32) CreatePen Error",
                       MB_OK);
            }

        memset(&lfTmp, 0, sizeof(lfTmp));
        lfTmp.lfPitchAndFamily = FIXED_PITCH;
        strcpy(lfTmp.lfFaceName, fontName);
        lfTmp.lfWeight       = FW_THIN;
        lfTmp.lfOutPrecision = OUT_TT_PRECIS;
        lfTmp.lfHeight       = fontHeightSmall;
        hSmallFont           = CreateFontIndirect(&lfTmp);
        if (!hSmallFont)
            {
            MessageBox(GetFocus(),
                       "Unable to get small height font ",
                       "(window_win32) CreateFont Error",
                       MB_OK);
            }

        memset(&lfTmp, 0, sizeof(lfTmp));
        lfTmp.lfPitchAndFamily = FIXED_PITCH;
        strcpy(lfTmp.lfFaceName, fontName);
        lfTmp.lfWeight       = FW_THIN;
        lfTmp.lfOutPrecision = OUT_TT_PRECIS;
        lfTmp.lfHeight       = fontHeightMedium;
        hMediumFont          = CreateFontIndirect(&lfTmp);
        if (!hMediumFont)
            {
            MessageBox(GetFocus(),
                       "Unable to get medium height font ",
                       "(window_win32) CreateFont Error",
                       MB_OK);
            }

        memset(&lfTmp, 0, sizeof(lfTmp));
        lfTmp.lfPitchAndFamily = FIXED_PITCH;
        strcpy(lfTmp.lfFaceName, fontName);
        lfTmp.lfWeight       = FW_THIN;
        lfTmp.lfOutPrecision = OUT_TT_PRECIS;
        lfTmp.lfHeight       = fontHeightLarge;
        hLargeFont           = CreateFontIndirect(&lfTmp);
        if (!hLargeFont)
            {
            MessageBox(GetFocus(),
                       "Unable to get large height font ",
                       "(window_win32) CreateFont Error",
                       MB_OK);
            }

        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_DESTROY:
        if (hSmallFont)
            {
            DeleteObject(hSmallFont);
            }
        if (hMediumFont)
            {
            DeleteObject(hMediumFont);
            }
        if (hLargeFont)
            {
            DeleteObject(hLargeFont);
            }
        if (hPen)
            {
            DeleteObject(hPen);
            }
        PostQuitMessage(0);
        break;

    case WM_TIMER:
        if (lpClipToKeyboard != NULL)
            {
            if (clipToKeyboardDelay == 0)
                {
                ppKeyIn = *lpClipToKeyboardPtr++;
                if (ppKeyIn == 0)
                    {
                    free(lpClipToKeyboard);
                    lpClipToKeyboard    = NULL;
                    lpClipToKeyboardPtr = NULL;
                    }
                else if (ppKeyIn == '\r')
                    {
                    clipToKeyboardDelay = 10;
                    }
                else if (ppKeyIn == '\n')
                    {
                    ppKeyIn = 0;
                    }
                }
            else
                {
                clipToKeyboardDelay -= 1;
                }
            }

        GetClientRect(hWnd, &rt);
        InvalidateRect(hWnd, &rt, TRUE);
        break;

    /*
    **  Paint the main window.
    */
    case WM_PAINT:
        windowDisplay(hWnd);
        break;

        /*
        **  Handle input characters.
        */
#if CcDebug == 1
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000)
            {
            switch (wParam)
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
                dumpRunningPpu((u8)(wParam - '0'));
                break;

            case 'C':
            case 'c':
                dumpRunningCpu();
                break;
            }
            }

        break;
#endif

    /*
     * Posted to the window with the keyboard focus when a WM_SYSKEYDOWN message
     * is translated by the TranslateMessage function. It specifies the character
     * code of a system character key that is, a character key that is pressed
     * while the ALT key is down.
     */
    case WM_SYSCHAR:
        switch (wParam)
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
            traceMask ^= (1 << (wParam - '0' + (shifted ? 10 : 0)));
            break;

        case 'C':
        case 'c':
            traceMask ^= TraceCpu;
            traceMask ^= TraceExchange;
            break;

        case 'E':
        case 'e':
            traceMask ^= TraceExchange;
            break;

        case 'X':
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

        case 'D':
        case 'd':
            traceMask ^= TraceCpu | TraceExchange | 2;
            break;

        case 'L':
        case 'l':
        case '[':
            displayMode           = ModeLeft;
            displayModeNeedsErase = TRUE;
            break;

        case 'R':
        case 'r':
        case ']':
            displayMode           = ModeRight;
            displayModeNeedsErase = TRUE;
            break;

        case 'M':
        case 'm':
        case '\\':
            displayMode = ModeCenter;
            break;

        case 'P':
        case 'p':
            windowClipboard(hWnd);
            break;

        case 's':
        case 'S':
            shifted = !shifted;
        }
        break;

    case WM_CHAR:
        ppKeyIn = (char)wParam;
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        }

    return 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Display current list.
**
**  Parameters:     Name        Description.
**                  hWnd        window handle.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void windowDisplay(HWND hWnd)
    {
    static   refreshCount = 0;
    char     str[2]       = " ";
    DispList *curr;
    DispList *end;
    long     oldFont = 0;

    RECT        rect;
    PAINTSTRUCT ps;
    HDC         hdc;
    HBRUSH      hBrush;

    HDC     hdcMem;
    HBITMAP hbmMem, hbmOld;
    HFONT   hfntOld;

    hdc = BeginPaint(hWnd, &ps);

    GetClientRect(hWnd, &rect);

    /*
    **  Create a compatible DC.
    */

    hdcMem = CreateCompatibleDC(ps.hdc);

    /*
    **  Create a bitmap big enough for our client rect.
    */
    hbmMem = CreateCompatibleBitmap(ps.hdc,
                                    rect.right - rect.left,
                                    rect.bottom - rect.top);

    /*
    **  Select the bitmap into the off-screen dc.
    */
    hbmOld = SelectObject(hdcMem, hbmMem);

    hBrush = CreateSolidBrush(colorBG);
    FillRect(hdcMem, &rect, hBrush);
    if (displayModeNeedsErase)
        {
        displayModeNeedsErase = FALSE;
        FillRect(ps.hdc, &rect, hBrush);
        }
    DeleteObject(hBrush);

    SetBkMode(hdcMem, TRANSPARENT);
    SetBkColor(hdcMem, colorBG);
    SetTextColor(hdcMem, colorFG);

    hfntOld = SelectObject(hdcMem, hSmallFont);
    oldFont = fontSmall;

#if CcCycleTime
        {
        extern double cycleTime;
        char          buf[80];

//    sprintf(buf, "Cycle time: %.3f", cycleTime);
        sprintf(buf, "Cycle time: %10.3f    NPU Buffers: %5d", cycleTime, npuBipBufCount());
        TextOut(hdcMem, 0, 0, buf, strlen(buf));
        }
#endif

#if CcDebug == 1
        {
        char buf[160];

        /*
        **  Display P registers of PPUs and CPU and current trace mask.
        */
        sprintf(buf, "Refresh: %-10d  PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o   CPU P-reg: %06o",
                refreshCount++,
                ppu[0].regP, ppu[1].regP, ppu[2].regP, ppu[3].regP, ppu[4].regP,
                ppu[5].regP, ppu[6].regP, ppu[7].regP, ppu[8].regP, ppu[9].regP,
                cpu.regP);

        sprintf(buf + strlen(buf), "   Trace0x: %c%c%c%c%c%c%c%c%c%c%c%c %c",
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
                traceMask & TraceCpu ? 'C' : '_',
                traceMask & TraceExchange ? 'E' : '_',
                shifted ? ' ' : '<');

        TextOut(hdcMem, 0, 0, buf, strlen(buf));

        if (ppuCount == 20)
            {
            /*
            **  Display P registers of second barrel of PPUs.
            */
            sprintf(buf, "                     PP P-reg: %04o %04o %04o %04o %04o %04o %04o %04o %04o %04o                    ",
                    ppu[10].regP, ppu[11].regP, ppu[12].regP, ppu[13].regP, ppu[14].regP,
                    ppu[15].regP, ppu[16].regP, ppu[17].regP, ppu[18].regP, ppu[19].regP);

            sprintf(buf + strlen(buf), "   Trace1x: %c%c%c%c%c%c%c%c%c%c%c%c %c",
                    (traceMask >> 10) & 1 ? '0' : '_',
                    (traceMask >> 11) & 1 ? '1' : '_',
                    (traceMask >> 12) & 1 ? '2' : '_',
                    (traceMask >> 13) & 1 ? '3' : '_',
                    (traceMask >> 14) & 1 ? '4' : '_',
                    (traceMask >> 15) & 1 ? '5' : '_',
                    (traceMask >> 16) & 1 ? '6' : '_',
                    (traceMask >> 17) & 1 ? '7' : '_',
                    (traceMask >> 18) & 1 ? '8' : '_',
                    (traceMask >> 19) & 1 ? '9' : '_',
                    ' ',
                    ' ',
                    shifted ? '<' : ' ');

            TextOut(hdcMem, 0, 12, buf, strlen(buf));
            }
        }
#endif

    if (opPaused)
        {
        static char opMessage[] = "(window_win32) Emulation paused";
        hfntOld = SelectObject(hdcMem, hLargeFont);
        oldFont = fontLarge;
        TextOut(hdcMem, (0 * scaleX) / 10, (256 * scaleY) / 10, opMessage, (int)strlen(opMessage));
        }
    else if (consoleIsRemoteActive())
        {
        static char opMessage[] = "Remote console active";
        hfntOld = SelectObject(hdcMem, hLargeFont);
        oldFont = fontLarge;
        TextOut(hdcMem, (0 * scaleX) / 10, (256 * scaleY) / 10, opMessage, (int)strlen(opMessage));
        }


    SelectObject(hdcMem, hPen);

    curr = display;
    end  = display + listEnd;
    for (curr = display; curr < end; curr++)
        {
        if (oldFont != curr->fontSize)
            {
            oldFont = curr->fontSize;

            if (oldFont == fontSmall)
                {
                SelectObject(hdcMem, hSmallFont);
                }

            if (oldFont == fontMedium)
                {
                SelectObject(hdcMem, hMediumFont);
                }

            if (oldFont == fontLarge)
                {
                SelectObject(hdcMem, hLargeFont);
                }
            }

        if (curr->fontSize == FontDot)
            {
            SetPixel(hdcMem, (curr->xPos * scaleX) / 10, (curr->yPos * scaleY) / 10 + 30, colorFG);
            }
        else
            {
            str[0] = curr->ch;
            TextOut(hdcMem, (curr->xPos * scaleX) / 10, (curr->yPos * scaleY) / 10 + 20, str, 1);
            }
        }

    listEnd  = 0;
    currentX = -1;
    currentY = -1;

    if (hfntOld)
        {
        SelectObject(hdcMem, hfntOld);
        }

    /*
    **  Blit the changes to the screen dc.
    */
    switch (displayMode)
        {
    default:
    case ModeCenter:
        BitBlt(ps.hdc,
               rect.left, rect.top,
               rect.right - rect.left, rect.bottom - rect.top,
               hdcMem,
               0, 0,
               SRCCOPY);
        break;

    case ModeLeft:
        StretchBlt(ps.hdc,
                   rect.left + (rect.right - rect.left) / 2 - 512 * scaleY / 10 / 2, rect.top,
                   512 * scaleY / 10, rect.bottom - rect.top,
                   hdcMem,
                   OffLeftScreen, 0,
                   512 * scaleX / 10 + fontLarge, rect.bottom - rect.top,
                   SRCCOPY);
        break;

    case ModeRight:
        StretchBlt(ps.hdc,
                   rect.left + (rect.right - rect.left) / 2 - 512 * scaleY / 10 / 2, rect.top,
                   512 * scaleY / 10, rect.bottom - rect.top,
                   hdcMem,
                   OffRightScreen, 0,
                   512 * scaleX / 10 + fontLarge, rect.bottom - rect.top,
                   SRCCOPY);
        break;
        }

    /*
    **  Done with off screen bitmap and dc.
    */
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(hWnd, &ps);
    }

/*---------------------------  End Of File  ------------------------------*/
