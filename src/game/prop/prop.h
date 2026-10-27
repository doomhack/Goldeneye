#ifndef _PROP_H_
#define _PROP_H_
#include <ultra64.h>
#include <bondtypes.h>

extern s16 * g_RoomPropsFound;
extern PropRecord g_Props[];
extern PropRecord *g_ActivePropsTail;
extern PropRecord *g_ActivePropsHead;
extern PropRecord *g_FreeProps;
extern s32                                g_OnScreenPropCount;
extern PropRecord **                      g_LastOnScreenProp;
extern PropRecord *                       g_OnScreenPropList[];
extern s16 *                              g_RoomFirstPropBlock;
extern struct roomproplistblock *         g_RoomPropBlocks;
void               propsTickOnce(void);
void               propsTickPerPlayer(void);
void               propsSortOnscreen(void);
void               autoaimUpdateTarget(void);
bool               playerInteract(void);
void               propsTickPickups(void);
void               propAddToRoom(PropRecord *, s16);
PropRecord*        propAllocate(void);
void               propReparent(PropRecord *newChild, PropRecord *host);
void               propDelist(PropRecord *prop);
void               propRemoveFromRooms(PropRecord *);
void               propCalcRooms(PropRecord *prop, coord3d *bbmin, coord3d *bbmax, f32 radius);
void               propAddToRooms(PropRecord *posData);
void               propActivate(PropRecord *);
void               propEnable(PropRecord *);
void               propActivateThisFrame(PropRecord *);
void               propGetCollisionPolygon(PropRecord *prop, struct rect4f **polygon, s32 *edges, f32 *top, f32 *bottom);
void               propSetMoving(PropRecord *prop, bool unset);
void               propGetCollisionPolygon2d(PropRecord *prop, struct rect4f **polygon, s32 *edges);
s32                polygonContainsPoint(coord3d *point, struct rect4f *polygon, s32 edges);
void               roomGetProps(s32 *roomids);
ObjectRecord      *objFindAtPad(s32 PadId);
Gfx               *propsRenderPass(Gfx *gdl, s32 roomid, s32 renderpass);
PropRecord        *propsGetActiveTail(void);
void               propsDefragRoomProps(void);
void               propGetRoomIds(PropRecord *self, s32 *roomids);
void               propFree(PropRecord *prop);
void               propDisable(PropRecord *prop);
f32                bboxGetYmin(ModelRoData_BoundingBoxRecord *modelBoundingBox);
f32                bboxGetYmax(ModelRoData_BoundingBoxRecord *modelBoundingBox);
f32                bboxGetZmin(ModelRoData_BoundingBoxRecord *modelBoundingBox);
ObjectRecord      *objFindAtPos(struct coord3d *pos, s32 RoomID);
void               propGetCollisionRadius(PropRecord *arg0, f32 *collision_radius, f32 *height, f32 *arg3);
f32                propGetGroundY(PropRecord *arg0);
f32                bboxRotatedMinX(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1);
f32                bboxRotatedMaxX(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1);
f32                bboxRotatedMinY(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1);
f32                bboxRotatedMaxY(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1);
f32                bboxRotatedMinZ(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1);
f32                bboxRotatedMaxZ(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1);
void               bboxGetHull2d(struct ModelRoData_BoundingBoxRecord *bbox, Mtxf *arg1, struct rect4f* arg2, struct collision_data *arg3);
void               projectRectCornersTo2D(struct coord3d *center, struct coord2d *arg1, struct coord2d *arg2, struct coord2d *arg3, struct coord2d *arg4);
void               switchActivateLinkedDoors(PropRecord *prop);
struct PropRecord *propFindForInteract(void);
void        propExecuteTickOperation(PropRecord *prop, TICKOP op);
void        propUpdateRooms(PropRecord* p);

void               propsInit(void);

#endif
