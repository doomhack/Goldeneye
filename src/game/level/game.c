/**
 * The game's one-time setup: gameInitData (the menu state at power on). Also the build date and two empty init
 * functions left from code Rare removed.
 */

#include <ultra64.h>
#include "game/asset/fileload.h"
#include <memp.h>
#include "game/level/game.h"
#include "game/menu/front.h"
#include <bondconstants.h>

// data


 /**
  * gameInitData
  *
  **/
void gameInitData(void) {
    g_FrontMenu = MENU_INVALID;
    g_FrontNextMenu = MENU_LEGAL_SCREEN;
    g_FrontEnterMenu = MENU_INVALID;
    g_FrontTimer = 0;
    g_FrontStage = LEVELID_NONE;
    g_FrontMission = BRIEFING_INVALID;
    g_FrontDifficulty = DIFFICULTY_MULTI;
    g_FrontScreenSize = SCREEN_SIZE_320x240;
    g_FrontFolderIcon = 0;
    g_FrontFolder = FOLDER1;
    g_FrontMissionAborted = FALSE;
    g_FrontFirstLegal = TRUE;
    g_FrontFirstBoot = TRUE;
    g_FrontSkipIntro = FALSE;
    g_FrontStartMusic = TRUE;
    g_Front007Reaction = 0.0f;
    g_Front007Health = 1.0f;
    g_Front007Damage = 1.0f;
    g_Front007Accuracy = 1.0f;
    g_CastIndex = 0;
    g_CastAnim = ANIM_idle;
    g_CastAnimCount = 0;
    g_CastModel = NULL;
    g_CastWeaponModel = NULL;
    g_CastFull = FALSE;
}


void null_init_main_2(void) {
    return;
}


void null_init_main_3(void) {
    return;
}
