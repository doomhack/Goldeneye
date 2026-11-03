#ifndef _BBOX_H_
#define _BBOX_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

ModelRoData_BoundingBoxRecord *modelFileFindBbox(const ModelFileHeader *header);
ModelRoData_BoundingBoxRecord *objFindBbox(const ObjectRecord *obj);

f32 bboxGetYmin(const ModelRoData_BoundingBoxRecord *bbox);
f32 bboxGetYmax(const ModelRoData_BoundingBoxRecord *bbox);
f32 bboxGetZmin(const ModelRoData_BoundingBoxRecord *bbox);

f32 bboxRotatedMinX(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx);
f32 bboxRotatedMaxX(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx);
f32 bboxRotatedMinY(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx);
f32 bboxRotatedMaxY(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx);
f32 bboxRotatedMinZ(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx);
f32 bboxRotatedMaxZ(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx);

s32 bboxGetHull2d(const ModelRoData_BoundingBoxRecord *bbox, const Mtxf *mtx, coord2d *hull);

bool padBoundsContainPoint(const coord3d *pos, f32 radius, const BoundPadRecord *pad);

void modelGetAxisExtents(const Model *model, f32 *max, f32 *min, s32 axis);
void modelGetXYExtents(const Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin);
void projectRectCornersTo2D(const coord3d *center, const coord2d *xrange, const coord2d *yrange, bbox2d *screen);

#endif
