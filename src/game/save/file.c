#include <ultra64.h>
#include "game/save/file.h"
#include "game/save/file2.h"
#include "game/menu/front.h"

f32 get_007_reaction_speed(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Reaction;
    }
    return 0.f;
}

f32 get_007_health_mod(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Health;
    }
    return 1.f;
}

f32 get_007_damage_mod(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Damage;
    }
    return 1.f;
}

f32 get_007_accuracy_mod(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Accuracy;
    }
    return 1.f;
}

void end_of_mission_briefing(void)
{
    s16 var1;

    if ((-1 < g_FrontMission) && g_FrontDifficulty != DIFFICULTY_007 && g_AppendCheatSinglePlayer == FALSE)
    {
        var1 = g_FrontTargetTimes[g_FrontMissions[g_FrontMission].mission_num][g_FrontDifficulty];
        fileUnlockStageInFolderAtDifficulty(g_FrontFolder, g_FrontMissions[g_FrontMission].mission_num, g_FrontDifficulty, lvGetMissionTime60() / 0x3c);
        if ((lvGetMissionTime60() / GAME_TICKRATE) <= var1)
        {
            if (!fileGetIsCheatUnlocked(fileGetSaveForFoldernum(g_FrontFolder), g_FrontMissions[g_FrontMission].mission_num))
            {
                fileSaveFolderUnlockCheat(g_FrontFolder, g_FrontMissions[g_FrontMission].mission_num);
                g_NewCheatUnlocked = TRUE;
                return;
            }
        }
#ifdef VERSION_US
        g_NewCheatUnlocked = FALSE;
#endif
    }
}

void fileLoadSaveSettingsForSelectedFolder(int stage)
{
    if(stage){
        #ifdef DEBUG

        #endif
    }
    fileLoadSettingsForFolder(g_FrontFolder);
}

void deleteCurrentSelectedFolder(void)
{
  fileClearSavefileForFolder(g_FrontFolder);
}

void fileUpdateBondInCurrentFolder(void)
{
  fileUpdateSelectedBondInSave(g_FrontFolder);
}

s32 fileGetBondForCurrentFolder(void)
{
  return fileGetBondForFolder(g_FrontFolder);
}

void fileSetCurrentFolder(u32 foldernum)
{
  g_FrontFolder = foldernum;
}

void set_selected_difficulty(DIFFICULTY difficulty)
{
    switch(difficulty)
    {
    case DIFFICULTY_AGENT:
    default:
        g_FrontDifficulty = DIFFICULTY_AGENT;
        return;
    case DIFFICULTY_SECRET:
        g_FrontDifficulty = DIFFICULTY_SECRET;
        return;
    case DIFFICULTY_00:
        g_FrontDifficulty = DIFFICULTY_00;
        return;
    case DIFFICULTY_007:
        g_FrontDifficulty = DIFFICULTY_007;
        return;
    }
}

void set_solo_and_ptr_briefing(LEVELID stage)
{
    g_FrontGameMode = GAMEMODE_SOLO;
    g_FrontStage = stage;
    g_FrontMission = frontFindMissionEntry(stage);
}

void sub_GAME_7F01D61C(struct save_data *savefile)
{
    fileCopySave(g_FrontFolder, savefile);
}



//this feels fake, generated with a permuter session i forgot was running
void set_selected_foldernum_and_copy_demo_eeprom(struct save_data *eeprom)
{
  int new_var;
  long long new_var2;
  new_var2 = (long long) 0x64;
  new_var = new_var2;
  g_FrontFolderSaved = (s32) g_FrontFolder;
  g_FrontFolder = new_var;
  fileCopyDemoSaveToRamRomSave(new_var, eeprom);
}


void store_favorite_weapon_current_player(u32 right,u32 left)
{
  u32 playerNum;

  playerNum = playerGetCurrentNum();
  g_MpFavWeapon[playerNum][GUNRIGHT] = right;
  g_MpFavWeapon[playerNum][GUNLEFT] = left;
}

