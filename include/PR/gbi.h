/*
 * The GBI (display list commands) for the game's graphics microcode, F3DEX3
 * (rsp/f3dex3/README.md).
 */
#ifndef GE_GBI_F3DEX3_EXTRA
#define GE_GBI_F3DEX3_EXTRA
#include <PR/gbi_f3dex3.h>

/* The sprite type that libultra's gu.h declares functions for (F3DEX3's GBI leaves it out) */
typedef struct {
    void *SourceImagePointer;
    void *TlutPointer;
    short Stride;
    short SubImageWidth;
    short SubImageHeight;
    char SourceImageType;
    char SourceImageBitSize;
    short SourceImageOffsetS;
    short SourceImageOffsetT;
    char dummy[4];
} uSprite_t;

typedef union {
    uSprite_t s;
    long long int force_structure_allignment[3];
} uSprite;
#endif
