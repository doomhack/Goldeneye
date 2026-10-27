# Save files

The four file folders, saved to the cartridge's EEPROM: which missions are done at which
difficulty, best times, unlocked cheats, and the settings.

| File | What it holds |
| --- | --- |
| `src/game/save/file.c` | Reading and checking the EEPROM, the save records (`g_Saves`), the per-folder questions the menus ask (`fileIsStageUnlockedAtDifficulty`, `fileGetIsCheatUnlocked`...), saving settings, the co-op partner's settings, and the checksum |

## What is in the EEPROM

The EEPROM starts with a small header (`smallSave`: a checksum and 24 bytes). This mod
stores the co-op partner's character and settings there (`fileCoopLoadPartnerSettings`).
After it come five save records (`save_data`):

* two checksums over the rest of the record (`fileGenerateCRC`);
* a flags byte packing the folder number, the slot, the Bond picture and a "needs reset"
  flag (`SAVEFLAGS_SET`);
* whether 007 mode is unlocked, the music and effects volumes, the settings bits
  (`OPTION_*`), and the unlocked cheats;
* the best times: 10 bits per level per difficulty (in seconds, so up to 1023), packed
  back to back. 007 isn't stored.

Four folders in five records: a record says which folder it belongs to, and
`fileGetSaveForFoldernum` searches for it. Saving a folder writes it to the spare record
with the next counter value (the "slot" in the flags, counting 0 to 3), so a write that
fails part way leaves the old copy behind. Copying a folder also writes to the spare
record (`fileCopyFolderToFirstFree`). A sixth record, only in RAM (`SAVESLOTRAMROM`),
holds the save a demo was recorded with.

`fileValidateSaves` runs at boot: it reads everything, resets records whose checksum
fails, keeps the newest copy when a folder has two, and makes a new record for a folder
that has none.

## What the rest of the game asks

* The front end: which missions and difficulties are open
  (`fileIsStageUnlockedAtDifficulty`), the best time, which cheats are unlocked.
* At the end of a mission, `fileRecordMissionEnd` saves the time, opens the next mission,
  and unlocks the level's cheat if the time beats the target.
* The settings: `fileLoadSettingsForFolder` copies a folder's settings into the watch
  menu's options (see [menu](menu.md)) and `fileSaveSettingsForFolder` writes them back.
* The 007 mode sliders (`file007Reaction` and friends): what the guards use on 007
  difficulty.

## Traps

* `file.c` also has a few functions that only touch front-end state
  (`fileSetDifficulty`, `fileSetSoloStage`, `fileSetFavWeapon`, the `file007*` getters).
  They belong in [menu](menu.md).
* `fileUnused1` and `fileUnused2` do nothing.
* The comment in `fileValidateSaves` calls reading from address 4 a bug, but EEPROM
  addresses count 8-byte blocks: address 4 is byte 32, just after the header.
