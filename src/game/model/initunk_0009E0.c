#include <ultra64.h>
#include "game/model/objecthandler.h"

/*
 * Address: 0x7F0009E0
*/
void animInitFrameBuffer(struct bondstruct_unk_animation_related* animBuffer, OSMesgQueue* mq, s32 unused) {
    g_AnimFrameBuffer = animBuffer; // Data structure with offset and two tables
    D_80036418 = mq;   // Unused message queue
    D_8003641C = unused; // Some unused array
}

