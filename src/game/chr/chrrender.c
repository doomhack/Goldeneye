/**
 * Drawing a guard: chrRenderProp draws the model (with fog fade and alpha), and
 * chrHandleJointPositioned bends the joints as they are placed: aiming up and to the
 * side, and the flinch when a shot is absorbed.
 */
#include <ultra64.h>
#include <memp.h>
#include <PR/gbi.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <macro.h>
#include <random.h>
#include <snd.h>
#include <math.h>
#include "bondaicommands.h"
#include "game/world/bg.h"
#include "game/menu/cheat.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chraction.h"
#include "game/model/chrobjdata.h"
#include "game/gfx/dyn.h"
#include "game/gfx/vtxstore.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/save/file.h"
#include "game/prop/propobj.h"
#include "game/prop/explosion.h"
#include "game/player/gun.h"
#include "game/model/anim.h"
#include "joy.h"
#include "game/level/lv.h"
#include "game/text/language.h"
#include "game/math/matrix.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/player/player.h"
#include "game/nav/stan.h"
#include "game/world/surface.h"
#include "game/math/physics.h"
#include "game/world/bgfog.h"
#include "game/math/line_tri_intersect.h"


static f32 chrGetFlinchAmount(ChrRecord *chr);


/**
 * Flinch animation envelope. Maps a character's flinchcnt timer to a normalized
 * intensity that goes 0->1 then falls 1->0, used to drive the flinch animation.
 * 
 * Flinches happen when a character has body armor or has the CHRFLAG_INVINCIBLE flag and is shot in the arm or torso.
 * 
 * Rise and fall are measured in VI retraces: 1/60 s on NTSC and 1/50 s on PAL.
 */
static f32 chrGetFlinchAmount(ChrRecord *chr)
{
    f32 count;
    f32 amount;
#if defined(VERSION_US)
    f32 rise = 10.0f;
    f32 fall = 20.0f;
#else
    f32 rise = 8.0f;
    f32 fall = 16.0f;
#endif

    count = chr->flinchcnt;

    if (count < rise)
    {
        amount = sinf(((count * M_TAU_F) * 0.25f) / rise);
    }
    else
    {
        amount = 1.0f - sinf((((count - rise) * M_TAU_F) * 0.25f) / fall);
    }

    return amount;
}


#define PI_OVER_3 1.0471976f
#define FIVEPI_OVER_18 0.87266463f


void chrHandleJointPositioned(enum CHR_RENDER_PART bodypart, Mtxf *matrix)
{
    f32 scale;
    f32 xrot;
    f32 yrot;
    f32 zrot;
    f32 tmp;
    f32 amount;
    f32 savedposz;
    f32 savedposy;
    f32 savedposx;
    f32 sideback;
    Mtxf rotmtx;
    u16 hidden;
    ChrRecord *chr;

    scale = 1.0f;

#ifdef BUGFIX_R1
    if (cheatIsActive(CHEAT_DK_MODE))
    {
        if (bodypart == CHR_RENDERPART_HEAD)
        {
            scale = 4.0f;
        }
        else if ((bodypart == CHR_RENDERPART_LEFT_ARM) || (bodypart == CHR_RENDERPART_RIGHT_ARM))
        {
            if (!(g_CurModelChr->chrflags & CHRFLAG_08000000))
            {
                scale = 2.5f;
            }
        }
    }
#else
    if (cheatIsActive(CHEAT_DK_MODE))
    {
        if (bodypart == CHR_RENDERPART_HEAD)
        {
            scale = 4.0f;
        }
        else if ((bodypart == CHR_RENDERPART_LEFT_ARM) || (bodypart == CHR_RENDERPART_RIGHT_ARM))
        {
            scale = 2.5f;
        }
    }
#endif
    if ((((bodypart != CHR_RENDERPART_LEFT_ARM) && (bodypart != CHR_RENDERPART_RIGHT_ARM)) && (bodypart != CHR_RENDERPART_TORSO)) && (bodypart != CHR_RENDERPART_HEAD))
    {
        return;
    }

    zrot = (yrot = (xrot = 0.0f));

#ifdef BUGFIX_R1
    chr = g_CurModelChr;
#endif
    if (bodypart == CHR_RENDERPART_RIGHT_ARM)
    {
#ifdef BUGFIX_R1
        xrot = chr->aimuprshoulder;
#else
        xrot = g_CurModelChr->aimuprshoulder;
#endif
    }
    else if (bodypart == CHR_RENDERPART_LEFT_ARM)
    {
#ifdef BUGFIX_R1
        xrot = chr->aimuplshoulder;
#else
        xrot = g_CurModelChr->aimuplshoulder;
#endif
    }
    else if (bodypart == CHR_RENDERPART_TORSO)
    {
#ifndef BUGFIX_R1
        chr = g_CurModelChr;
#endif
        xrot = chr->aimupback;
        if (chr->hidden & CHRHIDDEN_0400)
        {
            if (PI_OVER_3 < xrot)
            {
                xrot -= PI_OVER_3;
            }
            else if (-FIVEPI_OVER_18 > xrot)
            {
                xrot += FIVEPI_OVER_18;
            }
            else
            {
                xrot = 0.0f;
            }
        }

        yrot = chr->aimsideback;

    }
    else if (bodypart == CHR_RENDERPART_HEAD)
    {
#ifndef BUGFIX_R1
        chr = g_CurModelChr;
#endif
        if (chr->hidden & CHRHIDDEN_0400)
        {
            xrot = chr->aimupback;

            if (chr->hidden & CHRHIDDEN_0400)
            {
                if (PI_OVER_3 < xrot)
                {
                    xrot = PI_OVER_3;
                }
                else if (xrot < -FIVEPI_OVER_18)
                {
                    xrot = -FIVEPI_OVER_18;
                }
            }
        }
        else if (chr->model->flip != GUNRIGHT)
        {
            xrot = chr->aimuplshoulder;
        }
        else
        {
            xrot = chr->aimuprshoulder;
        }
    }

    chr = g_CurModelChr;

    if (chr->flinchcnt >= 0)
    {

        if ((bodypart == CHR_RENDERPART_RIGHT_ARM) || (bodypart == CHR_RENDERPART_LEFT_ARM))
        {
            amount = chrGetFlinchAmount(chr) * M_TAU_F * 15.0f / 360.0f;
            chr = g_CurModelChr;
            hidden = chr->hidden;
            xrot -= amount;

            if (hidden & CHRHIDDEN_RAND_FLINCH_1)
            {
                yrot -= amount;
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_2)
            {
                yrot += amount;
            }
        }
        else if (bodypart == CHR_RENDERPART_TORSO)
        {
            tmp = chrGetFlinchAmount(chr);
            tmp *= M_TAU_F;
            amount = tmp * 15.0f;
            chr = g_CurModelChr;
            hidden = chr->hidden;
            amount /= 360.0f;
            xrot += amount;

            if (hidden & CHRHIDDEN_RAND_FLINCH_1)
            {
                yrot += amount;
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_2)
            {
                yrot -= amount;
            }

            if (hidden & CHRHIDDEN_RAND_FLINCH_4)
            {
                 zrot += (tmp * 10.0f) / 360.0f;
            }
            else if (hidden & CHRHIDDEN_RAND_FLINCH_8)
            {
                zrot -= (tmp * 10.0f) / 360.0f;
            }
        }
    }

    if ((((xrot == 0.0f) && (yrot == 0.0f)) && (zrot == 0.0f)) && (scale == 1.0f))
    {
        return;
    }

    sideback = chrGetSubrotySideback(chr);

    if (xrot < 0.0f)
    {
        xrot = -xrot;
    }
    else
    {
        xrot = M_TAU_F - xrot;
    }

    if (yrot < 0.0f)
    {
        yrot += M_TAU_F;
    }

    mtx4MultAffineInPlace(camGetViewToWorldMtxf(), matrix);

    savedposx = matrix->m[3][0];
    savedposy = matrix->m[3][1];
    savedposz = matrix->m[3][2];

    matrix->m[3][0] = 0.0f;
    matrix->m[3][1] = 0.0f;
    matrix->m[3][2] = 0.0f;

    if ((xrot != 0.0f) || (zrot != 0.0f))
    {
        yrot -= sideback;

        if (yrot < 0.0f)
        {
            yrot += M_TAU_F;
        }

        mtx4LoadYRotation(yrot, &rotmtx);
        mtx4MultAffineInPlace(&rotmtx, matrix);

        if (xrot != 0.0f)
        {
            mtx4LoadXRotation(xrot, &rotmtx);
            mtx4MultAffineInPlace(&rotmtx, matrix);
        }

        if (zrot != 0.0f)
        {
            mtx4LoadZRotation(zrot, &rotmtx);
            mtx4MultAffineInPlace(&rotmtx, matrix);
        }

        mtx4LoadYRotation(sideback, &rotmtx);
        mtx4MultAffineInPlace(&rotmtx, matrix);
    }
    else
    {
        mtx4LoadYRotation(yrot, &rotmtx);
        mtx4MultAffineInPlace(&rotmtx, matrix);
    }

    if (scale != 1.0f)
    {
        mtx4ScaleRows012(scale, matrix);
    }

    matrix->m[3][0] = savedposx;
    matrix->m[3][1] = savedposy;
    matrix->m[3][2] = savedposz;

    mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), matrix);
}


#undef PI_OVER_3
#undef FIVEPI_OVER_18


Gfx *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    ChrRecord *chr;
    Model *chrmodel;
    rgba_f32 fogColour; // 192
    s32 fogType; // 188
    s32 renderFlags; // 184
    s32 chrfadealpha; // 180
    ModelRenderData mrData; // 112
    bbox2d bbox;
    rgba_s32 chrShade;
    s32 shadowAlpha; // 76
    PropRecord *prop_held_right; // 72
    PropRecord *prop_held_left; // 68
    PropRecord *prop_held_hat; // 64
    ObjectRecord *held_right_obj; // 60
    ObjectRecord *held_left_obj; // 56
    ObjectRecord *held_hat_obj; // 52

    //

    chr = prop->chr;
    chrmodel = chr->model;
    chrfadealpha = (s32) chr->fadealpha;

    if (!(chr->chrflags & CHRFLAG_04000000))
    {
        f32 f = propGetFogFade(prop, modelGetRadius(chrmodel)); //0-1
        chrfadealpha = (s32) (f * (f32) chrfadealpha);
    }

    if ((chrfadealpha < 0xFF) || (chr->chrflags & CHRFLAG_00020000))
    {
        if (withalpha == 0)
        {
            // nothing to do
            return gdl;
        }
        else
        {
            renderFlags = 3;
        }
    }
    else
    {
        if (withalpha == 0)
        {
            renderFlags = 1;
        }
        else
        {
            renderFlags = 2;
        }
    }

    fogType = fogGetPropDistColor(prop, &fogColour);
    if (fogType != 0)
    {
        if (chrfadealpha > 0)
        {
            mrData = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};


            shadowAlpha = 0x50;

            prop_held_right = chr->weapons_held[GUNRIGHT];
            prop_held_left = chr->weapons_held[GUNLEFT];
            prop_held_hat = chr->handle_positiondata_hat;
            held_right_obj = NULL;
            held_left_obj = NULL;
            held_hat_obj = NULL;

            if (prop_held_right != NULL)
            {
                held_right_obj = prop_held_right->obj;
            }

            if (prop_held_left != NULL)
            {
                held_left_obj = prop_held_left->obj;
            }

            if (prop_held_hat != NULL)
            {
                held_hat_obj = prop_held_hat->obj;
            }

            if ((propGetRoomsBbox2d(prop, &bbox) > 0) && !(chr->chrflags & CHRFLAG_CULL_USING_HITBOX))
            {
                gdl = bgScissorCurrentPlayerViewF(gdl, bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y);
            }
            else
            {
                gdl = bgScissorCurrentPlayerViewDefault(gdl);
            }

            mrData.flags = renderFlags;
            mrData.zbufferenabled = TRUE;
            mrData.gdl = gdl;

            if ((chr->chrflags & CHRFLAG_NO_SHADOW) != 0)
            {
                shadowAlpha = 0;
            }
            else if (fogType == 1)
            {
                shadowAlpha = ((1.0f - fogColour.a) * (f32)(shadowAlpha));
            }

            modelSetShadowAlpha(shadowAlpha);

            chrShade.r = chr->shadecol.r;
            chrShade.g = chr->shadecol.g;
            chrShade.b = chr->shadecol.b;
            chrShade.a = chr->shadecol.a;

            shadingLerp(&chrShade, fogType, &fogColour);

            mrData.envcolour.word = ((gBloodColour.rgba[0] << 0x18) | (gBloodColour.rgba[1] << 0x10)) | (gBloodColour.rgba[2] << 0x08);
            mrData.fogcolour.word = (chrShade.rgba[0] << 0x18) | (chrShade.rgba[1] << 0x10) | (chrShade.rgba[2] << 0x08) | (chrShade.rgba[3] << 0x00);

            if (chrfadealpha < 0xFF)
            {
                mrData.PropType = 8;
                mrData.envcolour.word |= (u8)chrfadealpha;
            }
            else
            {
                mrData.PropType = 7;
            }

            g_playerPerm->time_other_players_on_screen += 1;
            jointListDraw(&mrData, chr->jointList);

            gdl = mrData.gdl;

            if ((held_right_obj != NULL) && (( held_right_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = impactsRenderOnProp(gdl, prop_held_right, withalpha);
            }

            if ((held_left_obj != NULL) && (( held_left_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = impactsRenderOnProp(gdl, prop_held_left, withalpha);
            }

            if ((held_hat_obj != NULL) && (( held_hat_obj->state & ((u8)(1 << withalpha) )) ))
            {
                gdl = impactsRenderOnProp(gdl, prop_held_hat, withalpha);
            }

            if (withalpha != 0)
            {
                mtxsF2L(chr->model->render_pos, chr->model->obj->numMatrices);

                if ((held_right_obj != NULL) && ((held_right_obj->runtime_bitflags & 0x800) == 0))
                {
                    mtxsF2L(held_right_obj->model->render_pos, held_right_obj->model->obj->numMatrices);
                }

                if ((held_left_obj != NULL) && ((held_left_obj->runtime_bitflags & 0x800) == 0))
                {
                    mtxsF2L(held_left_obj->model->render_pos, held_left_obj->model->obj->numMatrices);
                }

                if (held_hat_obj != NULL)
                {
                    mtxsF2L(held_hat_obj->model->render_pos, held_hat_obj->model->obj->numMatrices);
                }
            }
        }
    }

    if (withalpha != 0)
    {
        jointListFree(chr->jointList);
        chr->jointList = NULL;
    }

    return gdl;
}

