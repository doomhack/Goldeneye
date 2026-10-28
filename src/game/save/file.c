/**
 * Save files: the four folders in the cartridge EEPROM (g_Saves), checking and repairing
 * them at boot, what each folder has completed and unlocked, best times, settings, the
 * co-op partner's settings and the checksum. See docs/subsystems/save.md.
 */
#include <ultra64.h>
#include <bondconstants.h>
#include "joy.h"
#include "str.h"
#include "game/player/player.h"
#include "game/menu/watchmenu.h"
#include "game/save/file.h"
#include "game/menu/front.h"
#include "game/menu/cheat.h"
#include <random.h>
#include "game/level/lv.h"

void fileGenerateCRC(u8 *addressA, u8 *addressB, s32 crc[2]);
s32 fileIs007ModeUnlocked(u32 folder);


// bss
save_data g_Saves[SAVESLOTMAX];

/**
 * The chr whose model is currently being built. Has to stay here to preserve ROM layout.
 */
ChrRecord *g_CurModelChr;

//data
s32 g_SaveBond[] = {BOND_BROSNAN,BOND_BROSNAN,BOND_BROSNAN,BOND_BROSNAN};


/**
 * @return s32
 */
s32 fileGamePakProbe(void)
{
  return joyGamePakProbe();
}

/**
 * Resets the RamRom replay folder save
 */
void fileResetRamRomSave(void)
{
    save_data new_save = BLANKSAVEDATA;

    g_Saves[SAVESLOTRAMROM] = new_save;
}

/**
 * @param save
 */
void fileWriteSmallSave(smallSave *save)
{
    if (fileGamePakProbe())
    {
        s32 crc[2];

        fileGenerateCRC(&save->data[0], &save->data[24], crc);
        save->chksum1 = crc[0];
        save->chksum2 = crc[1];
        joyGamePakLongWrite(0, save, sizeof(smallSave));
    }
}

/**
 * @param save
 */
void fileWriteSave(save_data *save)
{
    if (save >= &g_Saves[SAVESLOT1] && save < &g_Saves[SAVESLOTRAMROM])
    {
        if ( fileGamePakProbe())
        {
            s32 crc[2];

            fileGenerateCRC(&save->completion_bitflags, (u8 *)(save + 1), crc);
            save->chksum1 = crc[0];
            save->chksum2 = crc[1];
            joyGamePakLongWrite((((u32)((save - &g_Saves[SAVESLOT1]) * 0x60) >> 3) + 4), save, sizeof(save_data)); // 0x60 = sizeof(save_data) be sure to manually update if save changes
        }
    }
}

/**
 * reset save to default
 *
 * @param save
 */
void fileResetSave(save_data *save)
{
    save_data new_save  = BLANKSAVEDATA;

    *save = new_save;
    fileWriteSave(save);
}

/**
 * Get the folder of save
 *
 * @param save
 * @return u32
 */
u32 fileGetSaveFolder(save_data *save)
{
  return save->completion_bitflags & SAVEFLAG_FOLDER;
}

/**
 * Clear then set save folder flag
 *
 * @param save
 * @param folder
 */
void fileSetSaveFoldernum(save_data *save, u32 folder)
{
    save->completion_bitflags &= ~SAVEFLAG_FOLDER;
    save->completion_bitflags |= folder & SAVEFLAG_FOLDER;
}

/**
 * Get save flag 0x18
 *
 * @param folder
 * @return u32
 */
u32 fileGetSaveFlagSlot(save_data *folder)
{
  return (folder->completion_bitflags & SAVEFLAG_SLOT) >> 3;
}

/**
 * Resets save flag 0x18
 *
 * @param folder
 * @param slot
 */
void fileResetSaveFlagSlot(save_data *folder, s32 slot)
{
    folder->completion_bitflags &= ~SAVEFLAG_SLOT;
    folder->completion_bitflags |= ((slot * 8) & SAVEFLAG_SLOT);
}

/**
 * Get the selected bond save flag
 *
 * @param folder
 * @return u32
 */
u32 fileGetSelectedBond(save_data *folder)
{
  return (folder->completion_bitflags & SAVEFLAG_BOND) >> 5;
}

/**
 * Set the selected bond save flag
 *
 * @param folder
 * @param bond
 */
void fileSetSelectedBond(save_data *folder, s32 bond)
{
    folder->completion_bitflags &= ~SAVEFLAG_BOND;
    folder->completion_bitflags |= ((bond << 5) & SAVEFLAG_BOND);
}

/**
 * Check if save has flag 0x80
 *
 * @param folder
 * @return TRUE/FALSE
 */
bool fileGetSaveFlagDoReset(save_data *folder)
{
  return ((folder->completion_bitflags & SAVEFLAG_DORESET) != FALSE);
}

/**
 * Toggle save flag 0x80
 * possibly wear levelling
 *
 * @param folder: folder to enable or disable flag
 * @param set: Enable flag if TRUE, Disable flag if FALSE
 */
void fileSetSaveFlagDoReset(save_data *folder, bool enable)
{
    if (enable)
    {
        folder->completion_bitflags |= SAVEFLAG_DORESET;
    }
    else
    {
        folder->completion_bitflags &= ~SAVEFLAG_DORESET;
    }
}

/**
 * Get completion time for stage at difficulty
 *
 * @param save
 * @param levelid
 * @param difficulty
 * @return best time for stage at difficulty
 */
s32 fileGetSaveStageDifficultyTime(save_data* save, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty)
{
    s32 offset;
    LEVEL_SOLO_SEQUENCE max_level;
    u32 time;
    s32 index;

    max_level = SP_LEVEL_MAX;
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX ) && (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_MAX))
    {
        if (difficulty == DIFFICULTY_007)
        {
            if ( fileIs007ModeUnlocked( fileGetSaveFolder(save)))
            {
                return 0x3FF; //max time
            }
            return 0;
        }

        offset = ((difficulty * max_level) + levelid) * 10; //startbit
        index = (offset >> 3);

        switch(7 - (offset & 7)) //bitmask
        {
            case 7: //no offset agent
                // first 10 bits 8 + 2                    1111 1111                                      1100 0000
                time = ((save->times[index] & 0xFF) << 2) | ((save->times[index + 1] & 0xc0) >> 6);
                break;
            case 5: //offset 2 secret agent
                // next 10 bits 6 + 4                     0011 1111                                      1111 0000
                time =  ((save->times[index] & 0x3f) << 4) | ((save->times[index + 1] & 0xf0) >> 4);
                break;
            case 3: //offset 4 00 agent
                // next 10 bits 4 + 6                     0000 1111                                      1111 1100
                time =  ((save->times[index] & 0xf) << 6) | ((save->times[index + 1] & 0xfc) >> 2);
                break;
            case 1: //offset 6 007
                // next 10 bits 2 + 8                     0000 0011                                      1111 1111
                time = ((save->times[index] & 0x3)  << 8) | ((save->times[index + 1] & 0xFFF));
                break;
            default:
                time = 0; // shouldnt reach
        }

        return time;
    }

    return 0;
}

/**
 * Set completion time for stage at difficulty
 *
 * @param save
 * @param levelid
 * @param difficulty
 * @param newtime
 */
void fileSetDifficultyStageTime(save_data *save, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty, s32 newtime)
{
    s32 offset;
    s32 index;
    LEVEL_SOLO_SEQUENCE max_level;

    max_level = SP_LEVEL_MAX;
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX ) && (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_007))
    {
        if (newtime == 0) {
            newtime = 0x4f;
        } else if (newtime > 0x3ff) {
            newtime = 0x3ff;
        }

        offset = ((difficulty * max_level) + levelid) * 10; //startbit
        index = (offset >> 3);

        switch(7 - (offset & 7)) //bitmask
        {
            case 7: //no offset 4 8 12 etc agent
                save->times[index] = 0;
                save->times[index + 1] &= 0xff3f;
                save->times[index] |= (newtime >> 2) & 0xff;
                save->times[index + 1] |= (newtime << 6) & 0xc0;
                break;
            case 5: //first offset 5 9 13 etc secret agent
                save->times[index] &= 0xffc0;
                save->times[index + 1] &= 0xff0f;
                save->times[index] |= ((newtime >> 4) & 0x3f);
                save->times[index + 1] |= (newtime << 4) & 0xf0;
                break;
            case 3: //second offset 6 10 14 etc 00 agent
                save->times[index] &= 0xfff0;
                save->times[index + 1] &= 0xff03;
                save->times[index] |= ((newtime >> 6) & 0xf);
                save->times[index + 1] |= (newtime << 2) & 0xfC;
                break;
            case 1: //third offset 7 11 15 etc 007
                save->times[index] &= 0xfffc;
                save->times[index + 1] = 0;
                save->times[index] |= ((newtime >> 8) & 3);
                save->times[index + 1] |= newtime & 0xfff;
                break;
            default:
                break;
        }
    }
}


/**
 * Check if stage is completed at difficulty for save
 *
 * @param folder
 * @param levelid
 * @param difficulty
 * @return is stage at diffiuclty completed
 */
bool fileGetSaveStageCompletedForDifficulty(save_data *folder, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty)
{
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX) && (difficulty >= DIFFICULTY_AGENT) && (difficulty <= DIFFICULTY_007))
    {
        return fileGetSaveStageDifficultyTime(folder, levelid, difficulty) != 0;
    }

    return FALSE;
}

/**
 * Updates time for stage at difficulty if better
 *
 * @param folder
 * @param levelid
 * @param difficulty
 * @param newtime
 */
void fileCheckSaveStageDifficultyTime(save_data *folder, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty, s32 newtime)
{
    if ((levelid >= SP_LEVEL_DAM) && (levelid < SP_LEVEL_MAX) && (difficulty >= DIFFICULTY_AGENT) && (difficulty <= DIFFICULTY_007))
    {
        s32 time = fileGetSaveStageDifficultyTime(folder, levelid, difficulty);

        if ((time == 0) || (newtime < time))
        {
            fileSetDifficultyStageTime(folder, levelid, difficulty, newtime);
        }
    }
}

/**
 * Check if cheat is unlocked
 *
 * @param save
 * @param cheat
 * @return bool
 */
bool fileGetIsCheatUnlocked(save_data *save, s32 cheat)
{
    s32 bits;

    if (cheat >= 0 && cheat < CHEAT_INPUT_BUFFER_SIZE)
    {
        bits = save->unlocked_cheats_1 | save->unlocked_cheats_3 << 0x18 | save->unlocked_cheats_3 << 0x10 | save->unlocked_cheats_2 << 8;
        return ((1 << cheat) & bits) != 0;
    }

    return FALSE;
}

/**
 * @param save
 * @param cheat
 */
void fileSetSaveCheatUnlocked(save_data *save, s32 cheat)
{
    u32 i;
    u32 temp;

    if (cheat >= 0 && cheat < CHEAT_INPUT_BUFFER_SIZE)
    {
        temp = 1 << (cheat);

        for(i = 0; temp > 0xff; i++)
        {
            temp = temp >> 8;
        }

        *(((u8 *)save + i + 0xe)) |= temp & 0xFFu; //save.unlocked_cheats_1[i] |= temp;
    }
}

/**
 * Get save in foldernum slot
 *
 * @param foldernum
 * @return save_data*
 */
save_data * fileGetSaveForFoldernum(u32 folder)
{
    int i;

    for (i = SAVESLOT1; i < SAVESLOTRAMROM; i++)
    {
        if ( fileGetSaveFlagDoReset(&g_Saves[i]) == FALSE &&
                fileGetSaveFolder(&g_Saves[i]) == folder)
        {
            return &g_Saves[i];
        }
    }

    if (folder == RAMROM_FOLDERNUM)
    {
        return &g_Saves[SAVESLOTRAMROM];
    }

    return NULL;
}

/**
 * See if any save has 0x80 flag
 *
 * @return s32
 */
s32 fileAnyFolderNeedsReset(void)
{
    s32 i;

    for(i = SAVESLOT1; i < SAVESLOTRAMROM; i++)
    {
        if ( fileGetSaveFlagDoReset(&g_Saves[i]))
        {
            return i;
        }
    }

    return -1;
}

/**
 * Resets save with 0x80 flag
 * Maybe clearing for copy or wear level
 *
 * @param folder
 */
void fileBuildWriteNewSave(u32 folder)
{
    s32 folder_with_flag;

    folder_with_flag = fileAnyFolderNeedsReset();

    if (folder_with_flag >= 0)
    {
        save_data new_save = BLANKSAVEDATA;
        g_Saves[folder_with_flag] = new_save;

        fileSetSaveFoldernum(&g_Saves[folder_with_flag], folder);
        fileSetSaveFlagDoReset(&g_Saves[folder_with_flag], FALSE);
        fileSetSelectedBond(&g_Saves[folder_with_flag], folder);
        fileWriteSave(&g_Saves[folder_with_flag]);
    }
}

void fileValidateSaves(void)
{
    bool checksumOK;
    smallSave joyChecksum;
    s32 crc[2];
    s32 i;

    if (fileGamePakProbe())
    {
        checksumOK = TRUE;

        // block read 32 bytes
        joyGamePakLongRead(0, &joyChecksum, sizeof(smallSave));

        // if customised file dont assume crc is ok
        if (joyChecksum.data[0] != SAVEFLAGS_SET(FOLDER3, SAVESLOT1, BOND_CONNERY, FALSE))
        {
            checksumOK = FALSE;
        }

        fileGenerateCRC(&joyChecksum.data[0], &joyChecksum.data[24], crc); //do checksum on 24 bytes of save data

        if ((crc[0] != joyChecksum.chksum1) || (crc[1] != joyChecksum.chksum2))
        {
            checksumOK = FALSE;
        }

        // bad checksum, create a new save and replace damaged one.
        if (!checksumOK)
        {
            smallSave NewSave = {0, 0, SAVEFLAGS_SET(FOLDER3, SAVESLOT1, BOND_CONNERY, FALSE)};
            joyChecksum = NewSave;
            fileWriteSmallSave(&joyChecksum);
        }

        fileCoopLoadPartnerSettings(&joyChecksum);

        // Block read 5 saves starting at address 4th byte (? bug: address must be multiple of 8 - return is -1)
        joyGamePakLongRead(4, g_Saves, sizeof(save_data) * 5);

        for (i = SAVESLOT1; i != SAVESLOTRAMROM; i++) //only != matches
        {
            bool checksumOK2 = TRUE;

            fileGenerateCRC(&g_Saves[i].completion_bitflags, (u8 *)&g_Saves[i + 1], crc); // do checksum on save data

            if ((crc[0] != g_Saves[i].chksum1) ||
                (crc[1] != g_Saves[i].chksum2))
            {
                checksumOK2 = FALSE;
            }

            if (!checksumOK2)
            {
                fileResetSave(&g_Saves[i]);
            }
        }

        for (i = FOLDER1; i < MAX_FOLDER_COUNT; i++)
        {
            s32 slot_2;
            s32 jif    = -1;
            s32 slot = -1;
            s32 j;

            // for each save
            for (j = SAVESLOT1; j < SAVESLOTRAMROM; j++)
            {
                // if save = folder and SAVEFLAG_DORESET set
                if (!fileGetSaveFlagDoReset(&g_Saves[j]) &&
                    fileGetSaveFolder(&g_Saves[j]) == i)
                {
                    if (jif < SAVESLOT1) // on first SAVEFLAG_DORESET do this
                    {
                        jif = j;
                        slot = fileGetSaveFlagSlot(&g_Saves[j]);
                    }
                    else
                    {
                        slot_2 = fileGetSaveFlagSlot(&g_Saves[j]);

                        if (slot_2 == (slot + 1) % 4)
                        {
                            fileResetSave(&g_Saves[jif]);
                            jif = j;
                            slot = slot_2;
                        }
                        else
                        {
                            fileResetSave(&g_Saves[j]);
                        }
                    }
                }
            }

            // SAVEFLAG_DORESET was not set
            if (jif < SAVESLOT1)
            {
                fileBuildWriteNewSave(i);
            }
        }

        for (i = FOLDER1; i < MAX_FOLDER_COUNT; i++)
        {
            save_data *save = fileGetSaveForFoldernum(i);

            if (save)
            {
                g_SaveBond[i] = fileGetSelectedBond(save);
            }
        }
    }
}

/**
 * Check if folder is valid
 *
 * @param folder
 * @return bool
 */
bool fileIsFolderValid(s32 folder)
{
    if ((folder >= FOLDER1) && (folder < MAX_FOLDER_COUNT))
    {
        return TRUE;
    }

    if (folder == RAMROM_FOLDERNUM)
    {
        return TRUE;
    }

    return FALSE;
}

/**
 * wrapper func - uses save if found
 * file fileIsStageUnlockedAtDifficulty calls fileIsSavedStageUnlockedAtDifficulty
 *
 * @param foldernum
 * @param levelid
 * @param difficulty
 * @return 0, 1, or 3 (STAGESTATUS_LOCKED, STAGESTATUS_UNLOCKED, STAGESTATUS_COMPLETED)
 */
STAGESTATUS fileIsStageUnlockedAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty)
{
    save_data* save;
    s32 i;

    if (( fileIsFolderValid(foldernum)) &&
        (levelid >= SP_LEVEL_DAM && levelid < SP_LEVEL_MAX) &&
        (difficulty >= DIFFICULTY_AGENT && difficulty < DIFFICULTY_MAX))
    {
        save = fileGetSaveForFoldernum(foldernum);

        if (save)
        {
            if ( fileGetSaveStageCompletedForDifficulty(save, levelid, difficulty))
            {
                return STAGESTATUS_COMPLETED; //found on first try, stage has been completed and a time saved.
            }

#ifdef UNLOCK_ALL_LEVELS
            return STAGESTATUS_UNLOCKED;
#endif

            if ((levelid == SP_LEVEL_AZTEC && difficulty < DIFFICULTY_SECRET) ||
                (levelid == SP_LEVEL_EGYPT && difficulty < DIFFICULTY_00))
            {
                return STAGESTATUS_LOCKED; //we cant possibly have a completed bonus stage below each set dificulty
            }

            //still cant find it, do a search (this is probably how a cheat can unlock stages without having to actualy do them all)
            for (i = difficulty; i < DIFFICULTY_MAX ; i++)
            {
                LEVEL_SOLO_SEQUENCE istage;
                for (istage = SP_LEVEL_DAM; istage < levelid; istage++)
                {
                    if (! fileGetSaveStageCompletedForDifficulty(save, istage, i))
                    {
                        break;
                    }
                }
                //if the first uncomplete stage is not less than current
                if (levelid <= istage)
                {
                    return STAGESTATUS_UNLOCKED;
                }
            }

            // if we still cant find it
            if ((difficulty < DIFFICULTY_007) && (levelid < SP_LEVEL_AZTEC))
            {
                for (i = difficulty; i < DIFFICULTY_MAX; i++)
                {
                    if ( fileGetSaveStageCompletedForDifficulty(save, levelid - 1, i))
                    {
                        return STAGESTATUS_UNLOCKED;
                    }
                }
            }

            if (difficulty < DIFFICULTY_007)
            {
                for (i = SP_LEVEL_DAM; i < SP_LEVEL_AZTEC; i++)
                {
                    if (! fileGetSaveStageCompletedForDifficulty(save, i, DIFFICULTY_AGENT))
                    {
                        break;
                    }
                }
                //this cant actually fire an it?
                if (i >= SP_LEVEL_AZTEC)
                {
                    for (i = DIFFICULTY_AGENT; i < difficulty; i++)
                    {
                        if (! fileGetSaveStageCompletedForDifficulty(save, levelid, i))
                        {
                            break;
                        }
                    }

                    if (difficulty <= i)
                    {
                        return STAGESTATUS_UNLOCKED;
                    }
                }
            }// difficulty < DIFFICULTY_007
        }// save

        // no save, current level is dam, its unlocked.
        if (levelid == SP_LEVEL_DAM)
        {
            return STAGESTATUS_UNLOCKED;
        }

    }
    // After all that the stage is not unlocked
    return STAGESTATUS_LOCKED;
}

/**
 * @param save1
 * @param save2
 */
void fileOverwriteSaveSlotWithNewSave(save_data *save1, save_data *save2)
{
    s32 folder_with_flag;
    s32 slot;

    slot = 0;
    folder_with_flag = fileAnyFolderNeedsReset();

    if (folder_with_flag >= 0)
    {
        g_Saves[folder_with_flag] = *save2;

        if (save1)
        {
            slot = (s32)( fileGetSaveFlagSlot(save1) + 1) % 4;
        }

        fileSetSaveFlagDoReset(&g_Saves[folder_with_flag], FALSE);
        fileResetSaveFlagSlot(&g_Saves[folder_with_flag], slot);
        fileWriteSave(&g_Saves[folder_with_flag]);

        if (save1)
        {
            fileResetSave(save1);
        }
    }
}

/**
 * @param foldernum
 * @param stage
 * @param difficulty
 * @param maxtime
 */
void fileUnlockStageInFolderAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE stage, DIFFICULTY difficulty, s32 newtime)
{
    if ((foldernum >= 0) && (foldernum < MAX_FOLDER_COUNT) &&
        (stage >= SP_LEVEL_DAM) && (stage < SP_LEVEL_MAX) &&
        (difficulty >= DIFFICULTY_AGENT) && (difficulty < DIFFICULTY_MAX))
    {
        save_data new_save = BLANKSAVEDATA;

        save_data *save = fileGetSaveForFoldernum(foldernum);
        s32 i;
        if (save) {
            new_save = *save;
        } else {
            fileSetSaveFoldernum(&new_save, foldernum);
        }

        for (i = difficulty; i >= DIFFICULTY_AGENT; i--)
        {
            if (i == difficulty)
            {
                fileCheckSaveStageDifficultyTime(&new_save, stage, i, newtime);
            }
            else
            {
                fileCheckSaveStageDifficultyTime(&new_save, stage, i, 99999999);
            }
        }

        fileOverwriteSaveSlotWithNewSave(&save[0], &new_save);
    }
}

/**
 * @param foldernum
 * @param cheat
 */
void fileSaveFolderUnlockCheat(s32 foldernum, s32 cheat)
{
    if ((foldernum >= FOLDER1) && (foldernum < MAX_FOLDER_COUNT) && (cheat >= 0) && (cheat < CHEAT_INPUT_BUFFER_SIZE))
    {
        save_data *save = fileGetSaveForFoldernum(foldernum);

        if (save && fileGetIsCheatUnlocked(save, cheat))
        {
           return;
        }

        {
            save_data new_save = BLANKSAVEDATA;

            if (save)
            {
                new_save = *save;
            }
            else
            {
                fileSetSaveFoldernum(&new_save, foldernum);
            }

            fileSetSaveCheatUnlocked(&new_save, cheat);
            fileOverwriteSaveSlotWithNewSave(save, &new_save);
        }
    }
}


/**
 * Get the highest stage and difficulty completed in folder
 *
 * @param foldernum
 * @param stage
 * @param difficulty
 */
void fileGetHighestStageDifficultyCompletedForFolder(s32 foldernum, LEVEL_SOLO_SEQUENCE *levelid, DIFFICULTY *difficulty)
{
    save_data *folder;
    LEVEL_SOLO_SEQUENCE stageid;
    DIFFICULTY difficultyid;

    folder = fileGetSaveForFoldernum(foldernum);

    if (folder)
    {
        for (difficultyid = DIFFICULTY_007; difficultyid >= DIFFICULTY_AGENT; difficultyid--)
        {
            for (stageid = SP_LEVEL_EGYPT; stageid >= SP_LEVEL_DAM; stageid--)
            {
                if ( fileGetSaveStageCompletedForDifficulty(folder, stageid, difficultyid))
                {
                    *levelid = stageid;
                    *difficulty = difficultyid;
                    return;
                }
            }
        }
    }
    *levelid = SP_LEVEL_DAM - 1;
    *difficulty = DIFFICULTY_MULTI;
}

/**
 * Get the highest stage unlocked in folder
 *
 * @param foldernum
 * @return LEVEL_SOLO_SEQUENCE
 */
LEVEL_SOLO_SEQUENCE fileGetHighestStageUnlockedForFolder(s32 foldernum)
{
    LEVEL_SOLO_SEQUENCE levelid;
    DIFFICULTY difficulty;

    if ( fileGetSaveForFoldernum(foldernum) != NULL)
    {
        for (levelid = SP_LEVEL_EGYPT; levelid >= SP_LEVEL_DAM; levelid--)
        {
            for (difficulty = DIFFICULTY_AGENT; difficulty < DIFFICULTY_MAX; difficulty++)
            {
                if ( fileIsStageUnlockedAtDifficulty(foldernum, levelid, difficulty))
                {
                    return levelid;
                }
            }
        }
    }
    return SP_LEVEL_DAM;
}

/**
 * Get the highest stage unlocked in any folder
 *
 * @return levelid
 */
LEVEL_SOLO_SEQUENCE fileGetHighestStageUnlockedAnyFolder(void)
{
    int folder;
    LEVEL_SOLO_SEQUENCE isfound;
    LEVEL_SOLO_SEQUENCE highest = SP_LEVEL_DAM;

    for (folder = FOLDER1; folder < MAX_FOLDER_COUNT; folder++)
    {
        isfound = fileGetHighestStageUnlockedForFolder(folder);
        if (highest < isfound)
        {
            highest = isfound;
        }
    }

    return highest;
}

/**
 * Check if cradle has been completed at any difficulty
 *
 * @param foldernum
 * @return bool
 */
bool fileIsCradleCompletedForFolder(s32 folder)
{
    return (fileIsStageUnlockedAtDifficulty(folder, SP_LEVEL_CRADLE, DIFFICULTY_AGENT) == STAGESTATUS_COMPLETED) ||
           (fileIsStageUnlockedAtDifficulty(folder, SP_LEVEL_CRADLE, DIFFICULTY_SECRET) == STAGESTATUS_COMPLETED) ||
           (fileIsStageUnlockedAtDifficulty(folder, SP_LEVEL_CRADLE, DIFFICULTY_00) == STAGESTATUS_COMPLETED);
}

/**
 * Check if aztec has been completed at secret or 00 difficulty
 *
 * @param folder
 * @return bool
 */
bool fileIsAztecCompletedOnSecretOr00ForFolder(s32 folder)
{
    return (fileIsStageUnlockedAtDifficulty(folder, SP_LEVEL_AZTEC, DIFFICULTY_SECRET) == STAGESTATUS_COMPLETED) ||
           (fileIsStageUnlockedAtDifficulty(folder, SP_LEVEL_AZTEC, DIFFICULTY_00) == STAGESTATUS_COMPLETED);
}

/**
 * Check if egypt is completed at 00 difficulty
 *
 * @param foldernum
 * @return bool
 */
bool fileIsEgyptCompletedOn00ForFolder(int foldernum)
{
    return fileIsStageUnlockedAtDifficulty(foldernum, SP_LEVEL_EGYPT, DIFFICULTY_00) == STAGESTATUS_COMPLETED;
}

/**
 * Check if cradle has been completed in any folder
 *
 * @return bool
 */
bool fileIsCradleCompletedAnyFolder(void)
{
    s32 folder;

    for (folder = FOLDER1; folder < MAX_FOLDER_COUNT; folder++)
    {
        if ( fileIsCradleCompletedForFolder(folder))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Check if aztec has been completed in any folder at secret or 00 difficulty
 *
 * @return bool
 */
bool fileIsAztecCompletedOnSecretOr00AnyFolder(void)
{
    s32 folder;

    for (folder = FOLDER1; folder < MAX_FOLDER_COUNT; folder++)
    {
        if ( fileIsAztecCompletedOnSecretOr00ForFolder(folder))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Check if Egypt has been completed in any folder at secret or 00 difficulty
 *
 * @return bool
 */
bool fileIsEgyptCompletedOn00AnyFolder(void)
{
    s32 folder;

    for (folder = FOLDER1; folder < MAX_FOLDER_COUNT; folder++)
    {
        if ( fileIsEgyptCompletedOn00ForFolder(folder))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Get bond for folder
 *
 * @param folder
 * @return u8
 */
u8 fileGetBondForFolder(u32 folder)
{

    return BOND_BROSNAN;
}

/**
 * Set the selected bond to folder object
 *
 * @param folder
 * @param bond
 */
void fileSetSelectedBondTofolder(s32 folder, s32 bond)
{
    if (folder < FOLDER1 || folder > FOLDER4)
    {
        return;
    }
    g_SaveBond[folder] = BOND_BROSNAN;
}

/**
 * Delete save at foldernum
 *
 * @param foldernum
 */
void fileDeleteSaveForFolder(s32 foldernum)
{
    save_data *save;
    LEVEL_SOLO_SEQUENCE levelid;
    DIFFICULTY difficulty;

    if (foldernum >= FOLDER1 && foldernum < MAX_FOLDER_COUNT)
    {
        save = fileGetSaveForFoldernum(foldernum);
        if (save)
        {
            fileGetHighestStageDifficultyCompletedForFolder(foldernum, &levelid, &difficulty);
            if ((levelid >= SP_LEVEL_DAM) && (difficulty >= DIFFICULTY_AGENT))
            {
                save_data new_save = BLANKSAVEDATA;
                *save = new_save;
                fileSetSaveFoldernum(save, foldernum);
                fileSetSaveFlagDoReset(save, FALSE);
                fileSetSelectedBond(save, foldernum);
                fileSetSelectedBondTofolder(foldernum, foldernum);
                fileWriteSave(save);
            }
        }
    }
}

/**
 * Copy save
 *
 * Copies selected save to the first free slot
 * if no free slot, do nothing
 * @param foldernum Current folder number
 */
void fileCopyFolderToFirstFree(s32 foldernum)
{
    save_data* save;
    LEVEL_SOLO_SEQUENCE levelid;
    DIFFICULTY difficulty;
    s32 other;

    if ((foldernum >= FOLDER1) && (foldernum < MAX_FOLDER_COUNT))
    {
        save = fileGetSaveForFoldernum(foldernum);
        if (save)
        {
            fileGetHighestStageDifficultyCompletedForFolder(foldernum, &levelid, &difficulty);
            if (levelid >= SP_LEVEL_DAM)
            {
                if (difficulty >= DIFFICULTY_AGENT) {
                    for(other = FOLDER1;other != MAX_FOLDER_COUNT; other++)
                    {
                            if (( fileGetSaveForFoldernum(other) == NULL) ||
                                ( fileGetHighestStageDifficultyCompletedForFolder(other, &levelid, &difficulty),
                                (levelid < SP_LEVEL_DAM) && (difficulty < DIFFICULTY_AGENT)))
                            {
                                break;
                            }
                    }

                    if ((s32)other < MAX_FOLDER_COUNT)
                    {
                        save_data new_save = BLANKSAVEDATA;
                        save_data *dest = fileGetSaveForFoldernum(other);
                        new_save = *save;
                        fileSetSaveFoldernum(&new_save, other);
                        fileSetSelectedBondTofolder(other, fileGetBondForFolder(foldernum));
                        fileOverwriteSaveSlotWithNewSave(dest, &new_save);
                    }
                }
            }
        }
    }
}

/**
 * Packs the current player's options (and the global screen options) into
 * the save file's option bits.
 */
static u16 fileGetCurrentPlayerOptions(void)
{
    u32 temp;
    u16 bits;

    bits = 0;

    if (optionsGetLookInverted())
    {
        bits = OPTION_INVERTLOOK;
    }

    if (optionsGetAutoAim())
    {
        bits |= OPTION_AUTOAIM;
    }

    if (optionsGetAimControl())
    {
        bits |= OPTION_AIMCONTROL;
    }

    if (optionsGetSightOnScreen())
    {
        bits |= OPTION_SIGHTONSCREEN;
    }

    if (optionsGetLookAhead())
    {
        bits |= OPTION_LOOKAHEAD;
    }

    if (optionsGetAmmoOnScreen())
    {
        bits |= OPTION_DISPLAYAMMO;
    }

    if (optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN)
    {
        bits |= OPTION_SCREENWIDE;
    }
    else if (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)
    {
        bits |= OPTION_SCREENCINEMA;
    }

    if (optionsGetScreenRatio() != SCREEN_RATIO_NORMAL)
    {
        bits |= OPTION_SCREENRATIO;
    }

    if (optionsGetHiRes())
    {
        bits |= OPTION_HIRES;
    }

    if (!optionsGetSplitStacked())
    {
        bits |= OPTION_SPLITSIDE;
    }

    if (optionsGetShowFps())
    {
        bits |= OPTION_FPS;
    }

    if (optionsGetDrawNear())
    {
        bits |= OPTION_DRAWNEAR;
    }

    temp = ((u16) (optionsGetControlStyle() << 8)) & OPTION_CONTROLTYPE;
    return bits | temp;
}

/**
 * Applies the per-player part of packed option bits (not the control style,
 * screen ratio or hi-res) to the current player.
 */
static void fileSetCurrentPlayerOptions(u16 options)
{
    optionsSetLookInverted((options & OPTION_INVERTLOOK) != FALSE);
    optionsSetAutoAim((options & OPTION_AUTOAIM) != FALSE);
    optionsSetAimControl((options & OPTION_AIMCONTROL) != FALSE);
    optionsSetSightOnScreen((options & OPTION_SIGHTONSCREEN) != FALSE);
    optionsSetLookAhead((options & OPTION_LOOKAHEAD) != FALSE);
    optionsSetAmmoOnScreen((options & OPTION_DISPLAYAMMO) != FALSE);

    if (options & OPTION_SCREENCINEMA)
    {
        optionsSetScreenSize(SCREEN_SIZE_CINEMA);
    }
    else if (options & OPTION_SCREENWIDE)
    {
        optionsSetScreenSize(SCREEN_SIZE_WIDESCREEN);
    }
    else
    {
        optionsSetScreenSize(SCREEN_SIZE_FULLSCREEN);
    }
}

/**
 * @param save
 */
void fileSaveSettingsForFolder(save_data *save)
{
    save->music_vol = optionsGetMusicVolume() >> 7;
    save->sfx_vol = (optionsGetFxVolume() >> 7);
    save->options = fileGetCurrentPlayerOptions();
}

/**
 * The control style saved in a folder's options.
 */
s32 fileGetFolderControlType(u32 folder)
{
    save_data *save = fileGetSaveForFoldernum(folder);

    if (save == NULL)
    {
        return CONTROLLER_CONFIG_HONEY;
    }

    return ((save->options & OPTION_CONTROLTYPE) >> 8) & 0xFFFF;
}

/**
 * Loads settings from save file
 *
 * @param folder
 */
void fileLoadSettingsForFolder(u32 folder)
{
    save_data *save;
    u16 options;

    save = fileGetSaveForFoldernum(folder);
    if (save)
    {
        optionsSetMusicVolume((save->music_vol << 7) | (save->music_vol >> 1));
        optionsSetFxVolume((save->sfx_vol << 7) | (save->sfx_vol >> 1));

        options = save->options;

        if (!isMpGame())
        {
            optionsSetControlStyle(((s32) (options & OPTION_CONTROLTYPE) >> 8) & 0xFFFF);
        }
        else
        {
            optionsSetControlStyle(CONTROLLER_CONFIG_HONEY);
        }

        fileSetCurrentPlayerOptions(options);
        optionsSetScreenRatio((options & OPTION_SCREENRATIO) != FALSE);
        optionsSetHiRes((options & OPTION_HIRES) != FALSE);
        optionsSetSplitStacked((options & OPTION_SPLITSIDE) == FALSE);
        optionsSetShowFps((options & OPTION_FPS) != FALSE);
        optionsSetDrawNear((options & OPTION_DRAWNEAR) != FALSE);
    }
}

/**
 * Player 2's settings: their options from the co-op watch, their control
 * style (shared with multiplayer, see g_MpControlStyle) and their co-op
 * character. Player 2 isn't tied to a save folder, so these live in the
 * spare bytes of the EEPROM header block:
 * unk[1] = PARTNER_SAVE_MAGIC, unk[2..3] = options, unk[4] = character.
 */
#define PARTNER_SAVE_MAGIC 0xC0

static smallSave s_SaveHeader;
static s32 s_SaveHeaderValid = FALSE;
static u16 s_CoopPartnerOptions;
static s32 s_CoopPartnerOptionsSet = FALSE;
static s32 s_CoopPartnerChar = 0;

void fileCoopLoadPartnerSettings(smallSave *header)
{
    s_SaveHeader = *header;
    s_SaveHeaderValid = TRUE;

    if (header->data[1] == PARTNER_SAVE_MAGIC)
    {
        s_CoopPartnerOptions = (header->data[2] << 8) | header->data[3];
        s_CoopPartnerOptionsSet = TRUE;
        s_CoopPartnerChar = header->data[4];
        g_MpControlStyle[PLAYER_2] = (s_CoopPartnerOptions & OPTION_CONTROLTYPE) >> 8;
    }
}

/**
 * Writes player 2's settings to the EEPROM header if they changed.
 */
void fileCoopSavePartnerSettings(void)
{
    u16 options = s_CoopPartnerOptions;

    if (!s_SaveHeaderValid)
    {
        return;
    }

    if (!s_CoopPartnerOptionsSet)
    {
        options = DEFAULT_OPTIONS;
    }

    options = (options & ~OPTION_CONTROLTYPE) | ((g_MpControlStyle[PLAYER_2] << 8) & OPTION_CONTROLTYPE);

    if ((s_SaveHeader.data[1] == PARTNER_SAVE_MAGIC)
        && (s_SaveHeader.data[2] == (options >> 8))
        && (s_SaveHeader.data[3] == (options & 0xFF))
        && (s_SaveHeader.data[4] == s_CoopPartnerChar))
    {
        return;
    }

    s_SaveHeader.data[1] = PARTNER_SAVE_MAGIC;
    s_SaveHeader.data[2] = options >> 8;
    s_SaveHeader.data[3] = options & 0xFF;
    s_SaveHeader.data[4] = s_CoopPartnerChar;
    fileWriteSmallSave(&s_SaveHeader);
}

s32 fileCoopGetPartnerChar(void)
{
    return s_CoopPartnerChar;
}

void fileCoopSetPartnerChar(s32 character)
{
    s_CoopPartnerChar = character;
}

/**
 * Called when player 2 closes their watch. Their control style goes to
 * g_MpControlStyle so that multiplayer uses it too.
 */
void fileCoopStorePartnerOptions(void)
{
    s_CoopPartnerOptions = fileGetCurrentPlayerOptions();
    s_CoopPartnerOptionsSet = TRUE;
    g_MpControlStyle[PLAYER_2] = optionsGetControlStyle();
    fileCoopSavePartnerSettings();
}

/**
 * Gives the current player (player 2 in co-op or multiplayer) player 2's
 * options. The control style is set from g_MpControlStyle.
 */
void fileCoopRestorePartnerOptions(void)
{
    if (s_CoopPartnerOptionsSet)
    {
        fileSetCurrentPlayerOptions(s_CoopPartnerOptions);
    }
}

/**
 * Saves only the shared display options (hi-res, split screen, draw
 * distance) to a folder. Used by the menus, where there is no current
 * player to take the other options from.
 */
void fileSaveDisplaySettingsForFolder(s32 folder)
{
    u16 bits = 0;

    if (folder >= FOLDER1 && folder < MAX_FOLDER_COUNT)
    {
        save_data *save = fileGetSaveForFoldernum(folder);
        save_data new_save;

        if (save == NULL)
        {
            return;
        }

        if (optionsGetHiRes())
        {
            bits |= OPTION_HIRES;
        }

        if (!optionsGetSplitStacked())
        {
            bits |= OPTION_SPLITSIDE;
        }

        if (optionsGetDrawNear())
        {
            bits |= OPTION_DRAWNEAR;
        }

        new_save = *save;
        new_save.options = (new_save.options & ~(OPTION_HIRES | OPTION_SPLITSIDE | OPTION_DRAWNEAR)) | bits;

        if (new_save.options != save->options)
        {
            fileOverwriteSaveSlotWithNewSave(save, &new_save);
        }
    }
}

/**
 * Resets the folder save to default
 *
 * @param folder
 */
void fileClearSavefileForFolder(s32 folder)
{
    if (folder >= FOLDER1 && folder < MAX_FOLDER_COUNT)
    {
        save_data *save = fileGetSaveForFoldernum(folder);

        save_data save_to_copy = BLANKSAVEDATA;
        save_data new_save;
        if (save)
        {
            save_to_copy = *save;
        }
        else
        {
            fileSetSaveFoldernum(&save_to_copy, folder);
        }

        new_save = save_to_copy;

        fileSaveSettingsForFolder(&new_save);

        if (memcmp(&new_save, &save_to_copy, sizeof(save_data)))
        {
            fileOverwriteSaveSlotWithNewSave(save, &new_save);
        }
    }
}

/**
 * Not confident in the naming of this one...
 *
 * @param folder
 */
void fileUpdateSelectedBondInSave(s32 folder)
{

    if (folder >= FOLDER1 && folder < MAX_FOLDER_COUNT)
    {
        save_data *save = fileGetSaveForFoldernum(folder);
        save_data new_save = BLANKSAVEDATA;

        if (save)
        {
            new_save = *save;
        }
        else
        {
            fileSetSaveFoldernum(&new_save, folder);
        }

        if (g_SaveBond[folder] != fileGetSelectedBond(&new_save))
        {
            fileSetSelectedBond(&new_save, g_SaveBond[folder]);
            fileOverwriteSaveSlotWithNewSave(save, &new_save);
        }
    }
}

/**
 * Copy folder save
 *
 * @param foldernum
 * @param out_save
 */
void fileCopySave(s32 folder, save_data *out_save)
{

    save_data *in_save = fileGetSaveForFoldernum(folder);

    if (in_save)
    {
        *out_save = *in_save;
    }
    else
    {
        save_data new_save = BLANKSAVEDATA;
        *out_save = new_save;
    }
}

/**
 * Copy save to RarRom replay save
 *
 * @param folder
 * @param save
 */
void fileCopyDemoSaveToRamRomSave(u32 folder, save_data *save)
{
    if (folder == RAMROM_FOLDERNUM)
    {
        g_Saves[SAVESLOTRAMROM] = *save;
    }
}

/**
 * Check if 007 mode is unlocked
 *
 * @param folder
 * @return bool
 */
s32 fileIs007ModeUnlocked(u32 folder)
{
    LEVEL_SOLO_SEQUENCE levelid;
    save_data* save;

#ifdef UNLOCK_ALL_LEVELS
    return TRUE;
#endif

    save = fileGetSaveForFoldernum(folder);

    if (save != NULL)
    {
        if ((save->flag_007 & 1))
        {
            return TRUE;
        }

        for (levelid = SP_LEVEL_DAM; levelid < SP_LEVEL_MAX; levelid++)
        {
            if (! fileGetSaveStageCompletedForDifficulty(save, levelid, DIFFICULTY_00))
            {
                break;
            }
        }

        if (levelid == SP_LEVEL_MAX)
        {
            return TRUE;
        }
    }

    return FALSE;
}


f32 file007Reaction(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Reaction;
    }
    return 0.f;
}

f32 file007Health(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Health;
    }
    return 1.f;
}

f32 file007Damage(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Damage;
    }
    return 1.f;
}

f32 file007Accuracy(void)
{
    if (lvGetDifficulty() == DIFFICULTY_007) {
        return g_Front007Accuracy;
    }
    return 1.f;
}

void fileRecordMissionEnd(void)
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

void fileLoadCurrentSettings(void)
{
    fileLoadSettingsForFolder(g_FrontFolder);
}

void fileClearCurrentFolder(void)
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

void fileSetDifficulty(DIFFICULTY difficulty)
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

void fileSetSoloStage(LEVELID stage)
{
    g_FrontGameMode = GAMEMODE_SOLO;
    g_FrontStage = stage;
    g_FrontMission = frontFindMissionEntry(stage);
}

void fileCopyCurrentSave(struct save_data *savefile)
{
    fileCopySave(g_FrontFolder, savefile);
}


/**
 * Switches to the ramrom demo's own save folder, after copying the demo's save into it.
 * The current folder is kept in g_FrontFolderSaved so it can be restored afterwards.
 */
void fileUseDemoSave(struct save_data *eeprom)
{
    g_FrontFolderSaved = g_FrontFolder;
    g_FrontFolder = RAMROM_FOLDERNUM;
    fileCopyDemoSaveToRamRomSave(RAMROM_FOLDERNUM, eeprom);
}


void fileSetFavWeapon(u32 right,u32 left)
{
  u32 playerNum;

  playerNum = playerGetCurrentNum();
  g_MpFavWeapon[playerNum][GUNRIGHT] = right;
  g_MpFavWeapon[playerNum][GUNLEFT] = left;
}


/**
 * Get CRC of bytes from addressA to addressB and save to retval.chksum1 and retval.chksum2
 *
 * @param addressA
 * @param addressB
 * @param retval
 * @return * void
 */
void fileGenerateCRC(u8 *addressA, u8 *addressB, s32 crc[2])
{
    u8 *byte;
    s32 shift      = 0; // Shift value
    s64 polynormal = 0x8F809F473108B3C1;
    s32 checksum1  = 0; // Final checksum #1
    s32 checksum2  = 0; // Final checksum #2

    for(byte = addressA; byte < addressB; byte++,shift += 7)
    {
        polynormal += *byte << (shift & 0xF) ;
        checksum1 ^= randomGetNextFrom(&polynormal);
    }

    for(byte = addressB - 1; byte >= addressA; byte--,shift += 3)
    {
        polynormal += *byte << (shift & 0xF) ;
        checksum2 ^= randomGetNextFrom(&polynormal);
    }
    crc[0] = checksum1;
    crc[1] = checksum2;
}
