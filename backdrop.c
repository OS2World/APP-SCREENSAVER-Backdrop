#define INCL_WIN
#define INCL_GPI
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include <process.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "backdrop.h"

HAB     hAB;
CHAR    szAppName[] = "Desktop Background";
ULONG   ScreenWidth;
ULONG   ScreenHeight;
ULONG   BitmapWidth;
ULONG   BitmapHeight;
PFNWP   pfnwpOldFrameProc;

#define STACK_SIZE 10000

void cdecl main (void)
    {
    SEL     Sel;
    PCH     StackBottom;
    int     main2_ThreadID;
    HSYSSEM hssm;
    USHORT  usError;

    /*----------------------------------------------------------------------*/
    /*  Set up a semaphore to make sure that only one copy of backdrop is   */
    /*  ever loaded and running.                                            */
    /*                                                                      */
    /*  If the semaphore is already set up, the usError will be             */
    /*  ERROR_ALREADYEXISTS                                                 */
    /*----------------------------------------------------------------------*/
    usError = DosCreateSem (CSEM_PUBLIC,
                            &hssm,
                            "\\sem\\backdrop.sem");
    if (usError != 0)
        {
        DosExit (EXIT_PROCESS, 1);
        }

    DosSemSet (hssm);

    /*----------------------------------------------------------------------*/
    /*  Start up the main application as a secondary thread of the loading  */
    /*  application.                                                        */
    /*                                                                      */
    /*  I use a second thread so that the stack is protected.  In this way  */
    /*  any stack overflows will cause a GP fault, rather that overwriting  */
    /*  the client data segment contents.                                   */
    /*----------------------------------------------------------------------*/
    DosAllocSeg (STACK_SIZE, &Sel, 0);
    StackBottom = MAKEP (Sel, 0);
    main2_ThreadID = _beginthread (main2, StackBottom, STACK_SIZE, 0L);

    while (TRUE)
        {
        DosSleep (-1L);
        }

    DosSemClear (hssm);

    DosExit (EXIT_PROCESS, 1);
    }

void cdecl far main2 (void far * ArgList)
    {
    HMQ     hMq;
    HWND    hWndClient;
    HWND    hWndFrame;
    QMSG    qMsg;
    ULONG   ulStyle = NULL;
    SWCNTRL swctl;
    HSWITCH hswitch;
    PID     pid;

    /*----------------------------------------------------------------------*/
    /*  Create the message queue and background window.                     */
    /*----------------------------------------------------------------------*/

    hAB = WinInitialize (0);
    hMq = WinCreateMsgQueue (hAB, 0);

    if (WinRegisterClass (hAB,
                          szAppName,
                          ClientWndProc,
                          CS_SIZEREDRAW,
                          0))
        {
        hWndFrame = WinCreateStdWindow (HWND_DESKTOP,
                                        FS_NOBYTEALIGN,
                                        &ulStyle,
                                        szAppName,
                                        "",
                                        0L,
                                        NULL,
                                        NULL,
                                        &hWndClient);
        if (hWndFrame)
            {
            /*--------------------------------------------------------------*/
            /*  Add an entry to the swicth list for the background app.     */
            /*--------------------------------------------------------------*/
            WinQueryWindowProcess (hWndFrame, &pid, NULL);

            swctl.hwnd          = hWndFrame;        /* window handle      */
            swctl.hwndIcon      = NULL;             /* icon handle        */
            swctl.hprog         = NULL;             /* program handle     */
            swctl.idProcess     = pid;              /* process identifier */
            swctl.idSession     = NULL;             /* session identifier */
            swctl.uchVisibility = SWL_GRAYED;       /* visibility         */
            swctl.fbJump        = SWL_NOTJUMPABLE;  /* jump indicator     */
     
            strcpy (swctl.szSwtitle, "Fancy Background");

            hswitch = WinAddSwitchEntry (&swctl);

            /*--------------------------------------------------------------*/
            /*  Normal windows message loop                                 */
            /*--------------------------------------------------------------*/
            while (WinGetMsg (hAB, &qMsg, (HWND) NULL, 0, 0))
                {
                WinDispatchMsg (hAB, &qMsg);
                }

            WinShowWindow (hWndFrame, FALSE);
            WinDestroyWindow (hWndFrame);
            }
        else
            {
            WinMessageBox (HWND_DESKTOP,
                        NULL,
                        "Could not create main window",
                        szAppName,
                        0,
                        MB_OK | MB_ICONEXCLAMATION);
            }
        }
    else
        {
        WinMessageBox (HWND_DESKTOP,
                       NULL,
                       "Could not register window class",
                       szAppName,
                       0,
                       MB_OK | MB_ICONEXCLAMATION);
        }

    WinDestroyMsgQueue (hMq);
    WinTerminate (hAB);
    DosExit (EXIT_PROCESS, 0);
    }

MRESULT EXPENTRY ClientWndProc (HWND hWnd, USHORT Message,
                                MPARAM mp1, MPARAM mp2)
    {
    HPS     hPS;
    RECTL   Rect;
    RECTL   rcl;
    BITMAPINFOHEADER BitmapInfo;
    static HBITMAP   Bitmap;

    switch  (Message)
        {
        case WM_CREATE:
            {
            /*--------------------------------------------------------------*/
            /*  Load the background bitmap, scaling to the background       */
            /*  window size on loading.                                     */
            /*--------------------------------------------------------------*/
            ScreenWidth  = WinQuerySysValue (HWND_DESKTOP, SV_CXSCREEN);
            ScreenHeight = WinQuerySysValue (HWND_DESKTOP, SV_CYSCREEN);
       
            hPS = WinGetPS (HWND_DESKTOP);
            Bitmap = GpiLoadBitmap (hPS,
                                    NULL,
                                    ID_BACKGROUND,
                                    ScreenWidth,
                                    ScreenHeight);
            WinReleasePS (hPS);
         
            GpiQueryBitmapParameters (Bitmap, &BitmapInfo);

            BitmapWidth  = BitmapInfo.cx;
            BitmapHeight = BitmapInfo.cy;

            /*--------------------------------------------------------------*/
            /*  Push the window to the bottom of the window pile.           */
            /*--------------------------------------------------------------*/
            WinSetWindowPos (WinQueryWindow (hWnd,
                                             QW_PARENT,
                                             FALSE),
                             HWND_BOTTOM,
                             (SHORT) 0,
                             (SHORT) 0,
                             (SHORT) ScreenWidth, 
                             (SHORT) ScreenHeight,
                             SWP_SIZE | SWP_MOVE | SWP_SHOW | SWP_ZORDER);

            /*--------------------------------------------------------------*/
            /*  Sub class the frame window to prevent it coming to the top. */
            /*--------------------------------------------------------------*/
            pfnwpOldFrameProc = WinSubclassWindow (WinQueryWindow (hWnd, QW_PARENT, FALSE),
                                                   FrameSubclassProc);
            break;
            }

        case WM_DESTROY:
            {
            /*--------------------------------------------------------------*/
            /*  Clear out the bitmap and un-subclass the frame window.      */
            /*--------------------------------------------------------------*/
            GpiDeleteBitmap (Bitmap);
         
            WinSubclassWindow (WinQueryWindow (hWnd, QW_PARENT, FALSE),
                               (* pfnwpOldFrameProc));
            break;
            }

        case WM_SEM2:
            {
            /*--------------------------------------------------------------*/
            /*                                                              */
            /*--------------------------------------------------------------*/
            WinStartTimer (hAB, hWnd, 1, 1500);
            break;
            }

        case WM_TIMER:
            {
            /*--------------------------------------------------------------*/
            /*  Push the window to the bottom of the pile.                  */
            /*--------------------------------------------------------------*/
            WinStopTimer (hAB, hWnd, 1);
            WinSetWindowPos (WinQueryWindow (hWnd, QW_PARENT, FALSE),
                            HWND_BOTTOM,
                            0,
                            0,
                            0,
                            0,
                            SWP_ZORDER);
            break;
            }

        case WM_QUERYTRACKINFO:
            {
            /*--------------------------------------------------------------*/
            /*  Prevent the user moving the window                          */
            /*--------------------------------------------------------------*/
            return (MRESULT)FALSE;
            }

        case WM_BUTTON1DOWN:
            {
            /*--------------------------------------------------------------*/
            /*  Push the window to the bottom of the pile.                  */
            /*--------------------------------------------------------------*/
            WinSetWindowPos (WinQueryWindow (hWnd, QW_PARENT, FALSE),
                             HWND_BOTTOM,
                             0,
                             0,
                             0,
                             0,
                             SWP_ZORDER);
            break;
            }

        case WM_BUTTON1UP:
        case WM_ACTIVATE:
            {
            /*--------------------------------------------------------------*/
            /*  Push the window to the bottom of the pile.                  */
            /*--------------------------------------------------------------*/
            WinSetWindowPos (WinQueryWindow (hWnd, QW_PARENT, FALSE),
                             HWND_BOTTOM,
                             0,
                             0,
                             0,
                             0,
                             SWP_ZORDER);
            return (WinDefWindowProc (hWnd, Message, mp1, mp2));
            break;
            }

        case WM_PAINT:
            {
            /*--------------------------------------------------------------*/
            /*  Draw the bitmap on the screen and push it to the bottom of  */
            /*  the pile.                                                   */
            /*--------------------------------------------------------------*/
            hPS = WinBeginPaint (hWnd, (HPS) NULL, &Rect);
            WinQueryWindowRect (hWnd, &rcl);
            WinSetPointer (HWND_DESKTOP,
                           WinQuerySysPointer (HWND_DESKTOP, SPTR_WAIT, FALSE));

            WinDrawBitmap (hPS,
                           Bitmap,
                           NULL,
                           (PPOINTL) &rcl,
                           CLR_BLACK,
                           CLR_BLACK,
                           DBM_NORMAL);

            WinSetPointer (HWND_DESKTOP,
                           WinQuerySysPointer (HWND_DESKTOP, SPTR_ARROW, FALSE));
            
            WinEndPaint (hPS);
            
            WinSetWindowPos (WinQueryWindow (hWnd, QW_PARENT, FALSE),
                             HWND_BOTTOM,
                             0,
                             0,
                             0,
                             0,
                             SWP_ZORDER);
            break;
            }

        default:
            {
            return (WinDefWindowProc (hWnd, Message, mp1, mp2));
            }
        }     
    return (MRESULT) 0;
    }

MRESULT EXPENTRY FrameSubclassProc (HWND    hWnd,
                                    USHORT  Message,
                                    MPARAM  mp1,
                                    MPARAM  mp2)
    {
    switch (Message)
        {
        case WM_QUERYTRACKINFO:
            {
            /*--------------------------------------------------------------*/
            /*  Prevent the user moving the window                          */
            /*--------------------------------------------------------------*/
            return (MRESULT)FALSE;
            }

        default:
            {
            return (MRESULT)(* pfnwpOldFrameProc) (hWnd, Message, mp1, mp2);
            }
        }
    }
