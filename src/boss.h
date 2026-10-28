#ifndef _BOSS_H_
#define _BOSS_H_
#include <ultra64.h>
#include <bondgame.h>

/** How much memory a level gets for each of its buffers, in KB. */
typedef struct LevelMemBudget
{
    LEVELID level;
    u16 gfxKB;      /* each display list buffer (dyn.c), per view */
    u16 vtxKB;      /* each vertex buffer (dyn.c), per view */
    u16 textureKB;  /* the shared texture pool (image.c) */
    u16 roomHeapKB; /* mema's heap, for room geometry */
} LevelMemBudget;

extern const LevelMemBudget *g_LevelMemBudget;

LEVELID bossGetStageNum(void);
void bossSetLoadedStage(LEVELID stage);
void bossRunTitleStage(void);
void bossReturnTitleStage(void);

void bossEntry(void);

#endif
