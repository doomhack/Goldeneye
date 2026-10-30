#ifndef _BBOX_H_
#define _BBOX_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

ModelRoData_BoundingBoxRecord *modelFileFindBbox(ModelFileHeader *header);
ModelRoData_BoundingBoxRecord *objFindBbox(ObjectRecord *obj);

f32 bboxGetYmin(ModelRoData_BoundingBoxRecord *bbox);
f32 bboxGetYmax(ModelRoData_BoundingBoxRecord *bbox);
f32 bboxGetZmin(ModelRoData_BoundingBoxRecord *bbox);

f32 bboxRotatedMinX(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);
f32 bboxRotatedMaxX(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);
f32 bboxRotatedMinY(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);
f32 bboxRotatedMaxY(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);
f32 bboxRotatedMinZ(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);
f32 bboxRotatedMaxZ(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx);

s32 bboxGetHull2d(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord2d *hull);

bool padBoundsContainPoint(coord3d *pos, f32 radius, BoundPadRecord *pad);

void modelGetAxisExtents(Model *model, f32 *max, f32 *min, s32 axis);
void modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin);
void projectRectCornersTo2D(coord3d *center, coord2d *xrange, coord2d *yrange, bbox2d *screen);

#endif
