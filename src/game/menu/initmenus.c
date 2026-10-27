#include <ultra64.h>
#include <memp.h>
#include "game/menu/front.h"
#include "game/menu/initmenus.h"
#include "game/menu/title.h"
#include <macro.h>

void frontInit(void)
{
    s32 i;
    g_FrontMenu = ~MENU_LEGAL_SCREEN;
    g_FrontEnterMenu = ~MENU_LEGAL_SCREEN;
    g_FrontScreenSize = SCREEN_SIZE_320x240;
    g_FrontHiResPending = TRUE;
    g_FrontLowResPending = FALSE;
    g_FrontFolderIcon = 0;
    g_FrontFolderErase = -1;
    g_FrontFolderEraseChoice = 1;
    g_FrontTabStartHighlight = FALSE;
    g_FrontTabNextHighlight = FALSE;
    g_FrontTabPrevHighlight = FALSE;
    g_FrontStartMusic = TRUE;

    if (g_FrontSkipIntro)
    {
        g_FrontNextMenu = MENU_FILE_SELECT;
    }

    if (g_FrontNextMenu < 0)
    {
        g_FrontNextMenu = MENU_FILE_SELECT;
    }

    g_FrontModelBuffer = (u8 *)mempAlloc(0x78000, MEMPOOL_STAGE); // allocation in bytes so cast to u8 from gfx

#if defined(VERSION_EU)
    g_FrontFrameBuffer = mempAlloc(0x55040, MEMPOOL_STAGE);
#else
    g_FrontFrameBuffer = mempAlloc(0x4b040, MEMPOOL_STAGE);
#endif

    g_FrontFrameBuffer = ALIGN64_V1(g_FrontFrameBuffer);

    for (i = 0; i < 4; i++)
    {
        g_FrontFolderModels[i] = NULL;
    }

    titleAllocMatrices();
}
