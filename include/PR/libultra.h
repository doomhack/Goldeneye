#ifndef _LIBULTRA_H
#define _LIBULTRA_H

#define TV_TYPE_NTSC 1
#define TV_TYPE_PAL 0
#define TV_TYPE_MPAL 2

#define RESET_TYPE_COLD_RESET 0
#define RESET_TYPE_NMI 1
#define RESET_TYPE_BOOT_DISK 2

extern u32 osTvType NO_SDATA;
extern u32 osRomBase NO_SDATA;
extern u32 osResetType NO_SDATA;
extern u32 osMemSize NO_SDATA;
extern u8 osAppNmiBuffer[64];

#endif /* _LIBULTRA_H */
