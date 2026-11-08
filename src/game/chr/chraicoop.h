#ifndef _CHRAICOOP_H_
#define _CHRAICOOP_H_
#include <ultra64.h>
#include <bondtypes.h>

/**
 * Co-op: an AI command being repeated over the players. One per aiRunList() run.
 */
struct CoopAiState
{
    s32 start;      // offset of the command being repeated, -1 if none
    s32 size;
    s32 mode;
    s32 base;       // player current before the command
    s32 next;       // index into players[] to run next
    s32 count;
    s32 players[4];
};

void coopAiReset(struct CoopAiState *st);
void coopAiBegin(struct CoopAiState *st, AIRecord *list, s32 offset, const ChrRecord *chr);
s32  coopAiEnd(struct CoopAiState *st, s32 *offset, ChrRecord *chr);
void coopAiActOnActivator(ChrRecord *chr);
void coopAiActOnThrower(ChrRecord *chr, const ObjectRecord *item);
void coopAiActOnPlayer(ChrRecord *chr, s32 player);
s32  coopAnyPlayerStillHasItem(ITEM_IDS item);
s32  counterOpTextBegin(void);

void aiForObject(PropDefHeaderRecord *Entityp);
s32  counterOpDoorIsScripted(DoorRecord *door);
s32  coopDoorKeptOpen(DoorRecord *door);
s32  chrIsOnScreenForAi(const ChrRecord *chr);

#endif
