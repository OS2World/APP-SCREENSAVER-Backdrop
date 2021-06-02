#define ID_BACKGROUND   1

MRESULT EXPENTRY ClientWndProc     (HWND    hWnd, 
                                    USHORT  Message, 
                                    MPARAM  mp1, 
                                    MPARAM  mp2);

MRESULT EXPENTRY FrameSubclassProc (HWND    hWnd, 
                                    USHORT  Message, 
                                    MPARAM  mp1, 
                                    MPARAM  mp2);

MRESULT EXPENTRY AboutBoxDlgProc   (HWND    hDlg, 
                                    USHORT  Message, 
                                    MPARAM  mp1, 
                                    MPARAM  mp2);

void cdecl far  main2              (void far * ArgList);

void cdecl      main               (void far * ArgList);
