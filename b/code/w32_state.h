#if !defined(W32_STATE_H)
/* ========================================================================
   Creator: Patrik Fjellstedt $
   ========================================================================*/
#define W32_STATE_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

struct W32State
{
    char *sourceDLL;
    char *targetDLL;
    char *pdbLockFile;

    HWND hwnd;
    u32 windowWidth;
    u32 windowHeight;
};

#endif
