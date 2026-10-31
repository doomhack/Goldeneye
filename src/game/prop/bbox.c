/**
 * Bounding boxes of models, turned into the world.
 *
 * A model's bounding box node (MODELNODE_OPCODE_BBOX) holds an axis-aligned box in the
 * model's own frame. Turned by a matrix, the box's extent along each world axis is
 * bboxRotatedMin/Max*, and its outline seen from above (on the x/z plane) is
 * bboxGetHull2d: the convex hull of its eight corners, which is the collision outline of
 * most objects.
 *
 * Also here: whether a point is inside a bound pad's box (padBoundsContainPoint), a
 * model's extents in view space, and projecting them to the screen.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/bbox.h"
#include "game/model/model.h"
#include "game/player/player.h"


/* The first bounding box node among the root's children, or failing that among its
   first child's children */
ModelRoData_BoundingBoxRecord *modelFileFindBbox(ModelFileHeader *header)
{
    ModelNode *first = header->RootNode->Child;

    if (first == NULL)
    {
        return NULL;
    }

    for (ModelNode *node = first; node != NULL; node = node->Next)
    {
        if (node->Opcode == MODELNODE_OPCODE_BBOX)
        {
            return &node->Data->BoundingBox;
        }
    }

    for (ModelNode *node = first->Child; node != NULL; node = node->Next)
    {
        if (node->Opcode == MODELNODE_OPCODE_BBOX)
        {
            return &node->Data->BoundingBox;
        }
    }

    return NULL;
}


ModelRoData_BoundingBoxRecord *objFindBbox(ObjectRecord *obj)
{
    return modelFileFindBbox(obj->model->obj);
}


f32 bboxGetYmin(ModelRoData_BoundingBoxRecord *bbox)
{
    return bbox->Bounds.ymin;
}


f32 bboxGetYmax(ModelRoData_BoundingBoxRecord *bbox)
{
    return bbox->Bounds.ymax;
}


f32 bboxGetZmin(ModelRoData_BoundingBoxRecord *bbox)
{
    return bbox->Bounds.zmin;
}


/**
 * The smallest (or largest) value along world axis `axis` of the box's corners, turned
 * by mtx (without its translation). For each of the box's axes the corner on the low or
 * high side is picked by the sign of the matrix entry.
 */
static f32 bboxRotatedExtent(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, s32 axis, bool max)
{
    f32 result = 0.0f;

    for (s32 i = 0; i < 3; i++)
    {
        f32 m = mtx->m[i][axis];
        bool uselow = max ? m <= 0.0f : m >= 0.0f;

        result += (uselow ? bbox->Bounds.f[i][0] : bbox->Bounds.f[i][1]) * m;
    }

    return result;
}


f32 bboxRotatedMinX(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    return bboxRotatedExtent(bbox, mtx, 0, FALSE);
}


f32 bboxRotatedMaxX(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    return bboxRotatedExtent(bbox, mtx, 0, TRUE);
}


f32 bboxRotatedMinY(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    return bboxRotatedExtent(bbox, mtx, 1, FALSE);
}


f32 bboxRotatedMaxY(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    return bboxRotatedExtent(bbox, mtx, 1, TRUE);
}


f32 bboxRotatedMinZ(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    return bboxRotatedExtent(bbox, mtx, 2, FALSE);
}


f32 bboxRotatedMaxZ(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx)
{
    return bboxRotatedExtent(bbox, mtx, 2, TRUE);
}


/* Add to the hull the first of the remaining corners that lies outside the edge from
   corner a to corner b, if there is one */
static void hullAddCornerOutside(f64 pts[8][2], s32 rem[8], s32 a, s32 b, coord2d *poly, s32 *count)
{
    for (s32 i = 0; i < 4; i++)
    {
        s32 c = rem[i];

        if (((pts[c][0] - pts[b][0]) * (pts[a][1] - pts[b][1])) < ((pts[a][0] - pts[b][0]) * (pts[c][1] - pts[b][1])))
        {
            poly[*count].x = pts[c][0];
            poly[*count].y = pts[c][1];
            (*count)++;
            return;
        }
    }
}


/* Add a corner to the hull */
static void hullAddCorner(f64 pts[8][2], s32 c, coord2d *poly, s32 *count)
{
    poly[*count].x = pts[c][0];
    poly[*count].y = pts[c][1];
    (*count)++;
}


/**
 * The outline on the x/z plane of the box from (x1, y1, z1) to (x2, y2, z2) turned by m
 * and moved by its translation: the convex hull of its eight corners, from 4 to 8 points
 * written to poly. Returns the number of points.
 *
 * The four corners furthest along -x, -z, +x and +z are always on the hull. Between each
 * two of them, at most one of the other corners can stick out.
 */
static s32 boxGetHull2d(f32 x1, f32 x2, f32 y1, f32 y2, f32 z1, f32 z2, Mtxf *m, coord2d *poly)
{
    f64 pts[8][2];
    s32 minx = 0;
    s32 maxx = 0;
    s32 minz = 0;
    s32 maxz = 0;
    // The corners other than the extremes: 4, or more when two extremes are the same
    // corner (a box with no width or depth). Only the first 4 are tested.
    s32 rem[8];
    s32 numrem = 0;
    s32 count = 0;
    f64 x1d = x1;
    f64 x2d = x2;
    f64 y1d = y1;
    f64 y2d = y2;
    f64 z1d = z1;
    f64 z2d = z2;
    f64 m00 = m->m[0][0];
    f64 m02 = m->m[0][2];
    f64 m10 = m->m[1][0];
    f64 m12 = m->m[1][2];
    f64 m20 = m->m[2][0];
    f64 m22 = m->m[2][2];

    // Corner i uses x2 when bit 2 is set, y2 for bit 1 and z2 for bit 0
    for (s32 i = 0; i < 8; i++)
    {
        f64 x = (i & 4) ? x2d : x1d;
        f64 y = (i & 2) ? y2d : y1d;
        f64 z = (i & 1) ? z2d : z1d;

        pts[i][0] = ((m00 * x) + (m10 * y)) + (m20 * z);
        pts[i][1] = ((m02 * x) + (m12 * y)) + (m22 * z);
    }

    // The extreme corners, with ties broken on the other axis
    for (s32 i = 1; i < 8; i++)
    {
        if ((pts[i][0] < pts[minx][0]) || ((pts[i][0] == pts[minx][0]) && (pts[i][1] < pts[minx][1])))
        {
            minx = i;
        }
    }

    for (s32 i = 1; i < 8; i++)
    {
        if ((pts[maxz][1] < pts[i][1]) || ((pts[i][1] == pts[maxz][1]) && (pts[i][0] < pts[maxz][0])))
        {
            maxz = i;
        }
    }

    for (s32 i = 1; i < 8; i++)
    {
        if ((pts[maxx][0] < pts[i][0]) || ((pts[i][0] == pts[maxx][0]) && (pts[maxx][1] < pts[i][1])))
        {
            maxx = i;
        }
    }

    for (s32 i = 1; i < 8; i++)
    {
        if ((pts[i][1] < pts[minz][1]) || ((pts[i][1] == pts[minz][1]) && (pts[minz][0] < pts[i][0])))
        {
            minz = i;
        }
    }

    for (s32 i = 0; i < 8; i++)
    {
        if (i != minx && i != maxx && i != maxz && i != minz)
        {
            rem[numrem++] = i;
        }
    }

    hullAddCorner(pts, minx, poly, &count);
    hullAddCornerOutside(pts, rem, minx, minz, poly, &count);
    hullAddCorner(pts, minz, poly, &count);
    hullAddCornerOutside(pts, rem, minz, maxx, poly, &count);
    hullAddCorner(pts, maxx, poly, &count);
    hullAddCornerOutside(pts, rem, maxx, maxz, poly, &count);
    hullAddCorner(pts, maxz, poly, &count);
    hullAddCornerOutside(pts, rem, maxz, minx, poly, &count);

    for (s32 i = 0; i < count; i++)
    {
        poly[i].x += m->m[3][0];
        poly[i].y += m->m[3][2];
    }

    return count;
}


/* The x/z outline of a bounding box node turned by mtx (see boxGetHull2d) */
s32 bboxGetHull2d(ModelRoData_BoundingBoxRecord *bbox, Mtxf *mtx, coord2d *hull)
{
    return boxGetHull2d(bbox->Bounds.xmin, bbox->Bounds.xmax, bbox->Bounds.ymin, bbox->Bounds.ymax, bbox->Bounds.zmin, bbox->Bounds.zmax, mtx, hull);
}


/**
 * Whether a point is inside a bound pad's box (in the pad's frame: look is its z axis,
 * up its y axis), grown by radius on every side.
 */
bool padBoundsContainPoint(coord3d *pos, f32 radius, BoundPadRecord *pad)
{
    f32 dx = pos->x - pad->pos.x;
    f32 dy = pos->y - pad->pos.y;
    f32 dz = pos->z - pad->pos.z;
    f32 side[3];
    f32 d;

    // The pad's x axis: up cross look
    side[0] = (pad->up.y * pad->look.z) - (pad->look.y * pad->up.z);
    side[1] = (pad->up.z * pad->look.x) - (pad->look.z * pad->up.x);
    side[2] = (pad->up.x * pad->look.y) - (pad->look.x * pad->up.y);

    d = (pad->look.z * dz) + ((dx * pad->look.x) + (dy * pad->look.y));

    if ((pad->bbox.zmax + radius < d) || (d < pad->bbox.zmin - radius))
    {
        return FALSE;
    }

    d = (pad->up.z * dz) + ((dx * pad->up.x) + (dy * pad->up.y));

    if ((pad->bbox.ymax + radius < d) || (d < pad->bbox.ymin - radius))
    {
        return FALSE;
    }

    d = (dx * side[0]) + (dy * side[1]) + (side[2] * dz);

    if ((pad->bbox.xmax + radius < d) || (d < pad->bbox.xmin - radius))
    {
        return FALSE;
    }

    return TRUE;
}


/**
 * The extent of a model along one axis (0 x, 1 y, 2 z) of its node matrices' frame
 * (view space once the model has been placed for drawing): the union of all its
 * bounding box nodes.
 */
void modelGetAxisExtents(Model *model, f32 *max, f32 *min, s32 axis)
{
    ModelNode *node = model->obj->RootNode;
    bool first = TRUE;

    while (node != NULL)
    {
        if ((node->Opcode & 0xff) == MODELNODE_OPCODE_BBOX)
        {
            ModelRoData_BoundingBoxRecord *bbox = &node->Data->BoundingBox;
            Mtxf *mtx = modelFindNodeMtx(model, node, 0);
            f32 hi = bboxRotatedExtent(bbox, mtx, axis, TRUE) + mtx->m[3][axis];
            f32 lo = bboxRotatedExtent(bbox, mtx, axis, FALSE) + mtx->m[3][axis];

            if (first || hi > *max)
            {
                *max = hi;
            }

            if (first || lo < *min)
            {
                *min = lo;
            }

            first = FALSE;
        }

        // Depth first: the child, else the next sibling of this node or an ancestor
        if (node->Child != NULL)
        {
            node = node->Child;
        }
        else
        {
            while (node != NULL)
            {
                if (node->Next != NULL)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}


void modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin, f32 *ymax, f32 *ymin)
{
    modelGetAxisExtents(model, xmax, xmin, 0);
    modelGetAxisExtents(model, ymax, ymin, 1);
}


/**
 * The screen rectangle of a view-space box: each side is the centre moved to that side's
 * x or y range, projected. y increases down the screen, so the top of the range is min.y.
 */
void projectRectCornersTo2D(coord3d *center, coord2d *xrange, coord2d *yrange, bbox2d *screen)
{
    coord3d corner;
    coord2d out;

    corner.f[0] = xrange->f[0];
    corner.f[1] = center->f[1];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &out);
    screen->min.x = out.f[0];

    corner.f[0] = xrange->f[1];
    corner.f[1] = center->f[1];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &out);
    screen->max.x = out.f[0];

    corner.f[0] = center->f[0];
    corner.f[1] = yrange->f[1];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &out);
    screen->min.y = out.f[1];

    corner.f[0] = center->f[0];
    corner.f[1] = yrange->f[0];
    corner.f[2] = center->f[2];
    camViewToScreen(&corner, &out);
    screen->max.y = out.f[1];
}
