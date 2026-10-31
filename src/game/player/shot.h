#ifndef _SHOT_H_
#define _SHOT_H_
#include <ultra64.h>
#include <bondtypes.h>

// Firing shots: finding what a bullet hits (background and props), punches, and the hit sounds

ALSoundState * *gunGetFreeSfxState(void);
void shotPlayHitPropSound(enum ITEM_IDS item, PropRecord *prop, s32 texture_index);
void shotPlayGlassSound(coord3d *pos);
void shotPlayHitSound(enum ITEM_IDS item, coord3d *pos, s32 texture_index);
s32 shotRayHitsRoomBbox(s32 room, coord3d *start, coord3d *dir);
s32 shotFindBgHitInVisibleRooms(coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *besthit, s32 bestroom);
s32 shotFindBgHitInConnectedRooms(s32 startroom, coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *hit);
s32 shotFindBgHit(coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *besthit);
void shotFireBullet(s32 hand);
void shotAddHit(struct ShotData *shotdata, PropRecord *prop, f32 dist, s32 hitpart, ModelNode *node, struct HitThing *hitthing, s32 room, ModelNode *bboxnode, Model *model, bool countsAsPenetration, s32 blocksFurtherHits);
void shotPunch(s32 hand, s32 item_id);
void shotFireHand(s32 hand);
void shotFireHands(void);

#endif
