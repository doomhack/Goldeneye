/**
 * The game's one-time setup: gameInit and gameInitData (the menu state at power on), and
 * the menu memory pool (MEMPOOL_ME) helpers. Also the build date and two empty init
 * functions left from code Rare removed.
 */

#include <ultra64.h>
#include <deb.h>
#include "game/asset/fileload.h"
#include <memp.h>
#include "game/level/game.h"
#include "game/menu/front.h"
#include <bondconstants.h>

// data
//D:8004EAE0
s32 D_8004EAE0[] = {0, 0, 0, 0};

void gameInit(void) {
    debTryAdd(&D_8004EAE0, "game_c_debug");
}

void gameResetMePool(void) {
    fileForgetMePool();
    mempResetPool(MEMPOOL_ME);
    fileForgetPool(MEMPOOL_ME);
}

void gameCloseMePool(void) {
    mempClosePool(MEMPOOL_ME);
    fileForgetPool(MEMPOOL_ME);
}


 /**
  * gameInitData
  *
  **/
void gameInitData(void) {
    current_menu = MENU_INVALID;
    menu_update = MENU_LEGAL_SCREEN;
    maybe_prev_menu = MENU_INVALID;
    g_MenuTimer = 0;
    selected_stage = LEVELID_NONE;
    briefingpage = BRIEFING_INVALID;
    selected_difficulty = DIFFICULTY_MULTI;
    screen_size = SCREEN_SIZE_320x240;
    folder_selection_screen_option_icon = 0;
    selected_folder_num = FOLDER1;
    mission_failed_or_aborted = FALSE;
    is_first_time_on_legal_screen = TRUE;
    is_first_time_on_main_menu = TRUE;
    prev_keypresses = FALSE;
    maybe_is_in_menu = TRUE;
    slider_007_mode_reaction = 0.0f;
    slider_007_mode_health = 1.0f;
    slider_007_mode_damage = 1.0f;
    slider_007_mode_accuracy = 1.0f;
    intro_character_index = 0;
    randomly_selected_intro_animation = ANIM_idle;
    intro_animation_count = 0;
    cast_model = NULL;
    cast_model_weapon = NULL;
    full_actor_intro = FALSE;
#ifdef DEBUG
    bodyLoad(BODY_Brosnan_Tuxedo, "char");
    bodyLoad(BODY_Male_Pierce_Bond_Tuxedo, "head");
    LoadItemModel(PROP_GOLDENEYELOGO);
    LoadItemModel(PROP_LEGALPAGE);
#endif
}


char *gameGetCompileTime(void) {
    return __DATE__" " __TIME__;
}


void null_init_main_2(void) {
    return;
}


void null_init_main_3(void) {
    return;
}
