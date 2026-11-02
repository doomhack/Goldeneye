/*
 * The GBI (display list commands) for the graphics microcode the game is built for:
 * Fast3D (the original), or F3DEX3 with `make UCODE=f3dex3`.
 */
#ifdef UCODE_F3DEX3
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
#else
#include <PR/gbi_fast3d.h>
#endif
