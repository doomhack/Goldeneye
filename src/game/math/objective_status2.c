#include <ultra64.h>
#include "math.h"
#include "bondconstants.h"
#include "bondtypes.h"
#include "random.h"
#include "game/math/matrixmath.h"

//D:80032300
coord3d g_ZeroCoord = { 0, 0, 0 };


void physRandomTumble(Mtxf *mtx)
{
	coord3d coord;
    coord = g_ZeroCoord;
#if defined(LEFTOVERDEBUG)
	coord.x = RANDOMFRAC() * M_TAU_F * 0.03125f - 0.09817477f;
	coord.y = RANDOMFRAC() * M_TAU_F * 0.03125f - 0.09817477f;
	coord.z = RANDOMFRAC() * M_TAU_F * 0.03125f - 0.09817477f;
#else
	coord.x = RANDOMFRAC() * 7.53982257f * 0.03125f - 0.11780973f;
	coord.y = RANDOMFRAC() * 7.53982257f * 0.03125f - 0.11780973f;
	coord.z = RANDOMFRAC() * 7.53982257f * 0.03125f - 0.11780973f;
#endif
	mtx4LoadRotation(&coord, mtx);
}


void physRandomLaunch(coord3d *coord, Mtxf *mtx)
{
	coord->x = RANDOMFRAC() * 1.6666666f * 4.0f - 3.3333333f;
	coord->y = RANDOMFRAC() * 1.6666666f * 4.0f;
	coord->z = RANDOMFRAC() * 1.6666666f * 4.0f - 3.3333333f;

	physRandomTumble(mtx);
}


void physFall(f32* arg0, f32* arg1, f32 arg2)
{
    f32 temp_f0;
    f32 temp_f2;
    temp_f0 = *arg1;
    temp_f2 = temp_f0;
    temp_f0 = temp_f2 - (arg2 * 0.2777778f);
    *arg0 += (arg2 * (temp_f2 + temp_f0)) * 0.5f;
    *arg1 = temp_f0;
}


void physMoveWithGravity(f32* arg0, f32* arg1, f32 arg2)
{
    physFall(arg0 + 1, arg1 + 1, arg2);
    arg0[0] += (arg2 * arg1[0]);
    arg0[2] += (arg2 * arg1[2]);
}


void physApplyTumble(struct Mtxf *arg0, struct Mtxf *arg1, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++) {
        mtx4MultAffineInPlace(arg1, arg0);
    }
}


void physStep(f32* arg0, f32* arg1, struct Mtxf* arg2, struct Mtxf* arg3, s32 arg4)
{
    physMoveWithGravity(arg0, arg1, (f32) arg4);
    physApplyTumble(arg2, arg3, arg4);
}
