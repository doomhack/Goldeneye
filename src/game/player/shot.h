#ifndef _SHOT_H_
#define _SHOT_H_
#include <ultra64.h>
#include <bondtypes.h>

// Firing shots: finding what a bullet hits (background and props), punches, and the hit sounds

ALSoundState * *gunGetFreeSfxState(void);
void shotPlayHitPropSound(enum ITEM_IDS item, const PropRecord *prop, s32 texture_index);
void shotPlayGlassSound(const coord3d *pos);
void shotPlayHitSound(enum ITEM_IDS item, const coord3d *pos, s32 texture_index);
s32 shotRayHitsRoomBbox(s32 room, const coord3d *start, const coord3d *dir);
s32 shotFindBgHitInVisibleRooms(const coord3d *from, const coord3d *to, const coord3d *dir, const coord3d *scaledDir, u8 *visited, struct HitThing *besthit, s32 bestroom);
s32 shotFindBgHitInConnectedRooms(s32 startroom, const coord3d *from, const coord3d *to, const coord3d *dir, const coord3d *scaledDir, u8 *visited, struct HitThing *hit);
s32 shotFindBgHit(const coord3d *from, const coord3d *to, const coord3d *dir, const coord3d *scaledDir, u8 *visited, struct HitThing *besthit);
void shotFireBullet(s32 hand);
void shotAddHit(struct ShotData *shotdata, PropRecord *prop, f32 dist, s32 hitpart, ModelNode *node, const struct HitThing *hitthing, s32 room, ModelNode *bboxnode, Model *model, bool countsAsPenetration, s32 blocksFurtherHits);
void shotPunch(s32 hand, s32 item_id);
void shotFireHand(s32 hand);
void shotFireHands(void);

#endif
