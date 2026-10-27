#ifndef _FILE_H_
#define _FILE_H_
#include <ultra64.h>

#include <bondconstants.h>

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

#endif
