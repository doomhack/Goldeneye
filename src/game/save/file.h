#ifndef _FILE_H_
#define _FILE_H_
#include <ultra64.h>

#include <bondconstants.h>
#include <bondtypes.h>

typedef struct save_data
{
  s32 chksum1;
  s32 chksum2;
  u8 completion_bitflags;
  u8 flag_007;
  u8 music_vol;
  u8 sfx_vol;
  u16 options;
  u8 unlocked_cheats_1;
  u8 unlocked_cheats_2;
  u8 unlocked_cheats_3;
  char padding;
  u8 times[(SP_LEVEL_MAX-1) * 4];
} save_data;

// Used by fileValidateSaves
typedef struct smallSave
{
    int chksum1;
    int chksum2;
    u8 unk[24];
} smallSave;

extern save_data g_Saves[6];

extern s32 g_SaveBond[];

f32 file007Reaction(void);
f32 file007Health(void);
f32 file007Damage(void);
f32 file007Accuracy(void);
void fileRecordMissionEnd(void);
void fileLoadCurrentSettings(int stage);
void fileClearCurrentFolder(void);
void fileUpdateBondInCurrentFolder(void);
s32 fileGetBondForCurrentFolder(void);
void fileSetCurrentFolder(u32 foldernum);
void fileSetDifficulty(DIFFICULTY difficulty);
void fileSetSoloStage(LEVELID stage);
void fileCopyCurrentSave(struct save_data *savefile);
void fileUseDemoSave(struct save_data *eeprom);
void fileSetFavWeapon(u32 right,u32 left);




/* EEPROM masks for in-game settings */
#define OPTION_INVERTLOOK    0x0001
#define OPTION_AUTOAIM       0x0002
#define OPTION_AIMCONTROL    0x0004
#define OPTION_SIGHTONSCREEN 0x0008
#define OPTION_LOOKAHEAD     0x0010
#define OPTION_DISPLAYAMMO   0x0020
#define OPTION_SCREENWIDE    0x0040
#define OPTION_SCREENRATIO   0x0080
#define OPTION_CONTROLTYPE   0x0700
#define OPTION_SCREENCINEMA  0x0800
#define OPTION_HIRES         0x1000
#define OPTION_SPLITSIDE     0x2000 // set = side by side; stacked is the default
#define OPTION_FPS           0x4000
#define OPTION_DRAWNEAR      0x8000

#define DEFAULT_OPTIONS (OPTION_AUTOAIM | OPTION_SIGHTONSCREEN | OPTION_LOOKAHEAD | OPTION_DISPLAYAMMO)

extern ChrRecord *g_CurModelChr;

u8 fileGetBondForFolder(u32 folder);
void fileValidateSaves(void);
bool fileGetIsCheatUnlocked(save_data *save, s32 cheat);
STAGESTATUS fileIsStageUnlockedAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty);
void fileUnlockStageInFolderAtDifficulty(s32 foldernum, LEVEL_SOLO_SEQUENCE levelid, DIFFICULTY difficulty, s32 newtime);
void fileSaveFolderUnlockCheat(s32 foldernum, s32 cheat);
void fileLoadSettingsForFolder(u32 folder);
s32 fileGetFolderControlType(u32 folder);
void fileSaveDisplaySettingsForFolder(s32 folder);
void fileCoopLoadPartnerSettings(smallSave *header);
void fileCoopSavePartnerSettings(void);
s32 fileCoopGetPartnerChar(void);
void fileCoopSetPartnerChar(s32 character);
void fileCoopStorePartnerOptions(void);
void fileCoopRestorePartnerOptions(void);
void fileDeleteSaveForFolder(s32 foldernum);

void fileGetHighestStageDifficultyCompletedForFolder(s32 foldernum, LEVEL_SOLO_SEQUENCE *levelid, DIFFICULTY *difficulty);
bool fileIsAztecCompletedOnSecretOr00AnyFolder(void);
bool fileIsEgyptCompletedOn00AnyFolder(void);
LEVEL_SOLO_SEQUENCE fileGetHighestStageUnlockedAnyFolder(void);

#endif
