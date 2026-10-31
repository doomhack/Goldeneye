/**
 * Beams and tracers: the streak a fired shot leaves (beamCreate,
 * gunCreateBeamForHand), moving it each frame and drawing it.
 */
#include "assets/obseg/text/LgunE.h"
#include "game/gfx/dyn.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texcmds.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/menu/watchmenu.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "gbi_extension.h"
#include "include/limits.h"
#include "random.h"
#include "game/player/beam.h"


/**
 * @param arg0:
 * @param item:
 * @param arg2:
 * @param arg3:
 *
 *
 * This function adjusts the length of the bullet beam that's rendered on screen.
 * This function is used for both player and guard beams.
 *
 * The watch laser has a very short beam, in accordance with its range.
 * The laser also has a shortened one, but it appears this is to avoid graphical glitches.
 * Other weapons have their bullet beam capped at 10000 max length, otherwise if the player
 * fires into the void, there may be graphical glitches with the beam.
*/
void beamCreate(struct BeamRecord *beam, ITEM_IDS item, coord3d *from, coord3d *to)
{
    f32 dist;

    beam->from.f[0] = from->f[0];
    beam->from.f[1] = from->f[1];
    beam->from.f[2] = from->f[2];

    beam->dir.f[0] = to->x - from->x;
    beam->dir.f[1] = to->f[1] - from->f[1];
    beam->dir.f[2] = to->f[2] - from->f[2];

    dist = sqrtf((beam->dir.f[0] * beam->dir.f[0]) + (beam->dir.f[1] * beam->dir.f[1]) + (beam->dir.f[2] * beam->dir.f[2]));

    beam->dir.f[0] *= 1.0f / dist;
    beam->dir.f[1] *= 1.0f / dist;
    beam->dir.f[2] *= 1.0f / dist;

    if (item == ITEM_WATCHLASER)
    {
        if (dist > 300.0f)
        {
            dist = 300.0f;
        }
    }
    else
    {
        if (dist > 10000.0f)
        {
            dist = 10000.0f;
        }
    }

    beam->age = 0;
    beam->weaponnum = (s8) item;
    beam->maxdist = dist;

    if (dist < 500.0f)
    {
        dist = 500.0f;
    }

    if (item == ITEM_LASER)
    {
        beam->speed = 0.25f * dist;
        beam->length = 0.6f * dist;

        if (beam->length > 3000.0f)
        {
            beam->length = 3000.0f;
        }

        // Laser beams are rendered more often than other normal weapons
        beam->dist = (-0.1f - ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX) * 0.3f)) * dist;
    }
    else if (item == ITEM_WATCHLASER)
    {
        beam->length = dist;
        beam->speed = 2.0f * dist;

        if (dist > 3000.0f)
        {
            beam->length = 3000.0f;
        }

        // Always render the beam for the watch laser
        beam->dist = 0.0f;
    }
    else
    {
        beam->speed = 0.2f * dist;
        beam->length = beam->speed;

        if (beam->speed > 3000.0f)
        {
            beam->length = 3000.0f;
        }

        // Decide if a beam should be rendered for normal weapon bullets
        beam->dist = ((2.0f * ((f32) (u32)randomGetNext() * (1.0f / UINT_MAX))) - 1.0f) * beam->speed;
    }

    if (beam->maxdist <= beam->dist)
    {
        // No beam will be rendered
        beam->age = -1;
    }
}


/**
 * Starts a tracer from the hand's gun towards what the shot hit.
 */
void gunCreateBeamForHand(enum GUNHAND hand)
{
    Mtxf *player_matrix;
    struct hand *hand_ptr;
    f32 val;
    struct ChrRecord *chr;
    f32 diff1_z;
    f32 diff1_y;
    f32 diff1_x;
    f32 diff2_z;
    f32 diff2_y;
    f32 diff2_x;
    BeamRecord *weapon_beam;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    player_matrix = camGetWorldToScreenMtxf();

    val = -((((hand_ptr->item_related.x * player_matrix->m[0][2]) + (hand_ptr->item_related.y * player_matrix->m[1][2])) + (hand_ptr->item_related.z * player_matrix->m[2][2])) + player_matrix->m[3][2]);

    if (val < hand_ptr->muzzleDepth)
    { 
        return; 
    }

    weapon_beam = &hand_ptr->weapon_beam;

    beamCreate(weapon_beam, gunGetWeapon(hand), &hand_ptr->muzzlePos, &hand_ptr->item_related);

    if ((g_CurrentPlayer->prop->chr == NULL) || (getPlayerCount() < 2)) 
    { 
        return; 
    }

    chr = g_CurrentPlayer->prop->chr;

    diff1_x = hand_ptr->item_related.x - g_CurrentPlayer->bodyHandPos[hand].x;
    diff1_y = hand_ptr->item_related.y - g_CurrentPlayer->bodyHandPos[hand].y;
    diff1_z = hand_ptr->item_related.z - g_CurrentPlayer->bodyHandPos[hand].z;
    guNormalize(&diff1_x, &diff1_y, &diff1_z);

    diff2_x = hand_ptr->item_related.x - hand_ptr->muzzlePos.x;
    diff2_y = hand_ptr->item_related.y - hand_ptr->muzzlePos.y;
    diff2_z = hand_ptr->item_related.z - hand_ptr->muzzlePos.z;
    guNormalize(&diff2_x, &diff2_y, &diff2_z);

    val = mathAcos(
        + (diff2_z * diff1_z)
        + ((diff1_x * diff2_x)
        + (diff1_y * diff2_y)));

    if (val > 0.08726647f) 
    { 
        return; 
    }

    beamCreate(&chr->beams[hand], gunGetWeapon(hand), &g_CurrentPlayer->bodyHandPos[hand], &hand_ptr->item_related);
}


/**
 * Draws a beam.
 */
Gfx *beamRender(Gfx *gdl, BeamRecord *flash, s32 zbufferMode)
{
    Mtx *mtx;
    Mtxf mtxf;
    Vtx templatevtx;
    coord3d pos;
    coord3d *playerpos;
    f32 radius;
    f32 startoffset;
    coord3d up;
    coord3d right;
    coord3d flareoffset;
    coord3d extraorigin;
    f32 extra_scale;
    struct sImageTableEntry *image;
    f32 d;
    Mtxf *worldtoscreen;
    coord3d tmp;
    coord3d divout;
    f32 divin[2];
    f32 f14;
    f32 f16;
    f32 f18;
    f32 dist_to_start;
    Vtx *vtx;
    f32 dist;
    Gfx *cmd;

    if (flash->age >= 0)
    {
        templatevtx = (Vtx){{{0, 0, 0}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}};
        playerpos = playerGetPos();
        startoffset = flash->dist;
        dist = flash->length;
        flareoffset = (coord3d){0};
        extraorigin = (coord3d){0};
        extra_scale = 1.4142f; // ~√2
        image = g_ImageFlare3;
        worldtoscreen = camGetWorldToScreenMtxf();

        if (flash->weaponnum == ITEM_LASER)
        {
            radius = 50.0f;
            image = g_ImageFlare4;
        }
        else if (flash->weaponnum == ITEM_WATCHLASER)
        {
            radius = 10.0f;
            image = g_ImageFlare4;
            templatevtx.v.cn[3] = (randomGetNext() % 50) + 150;

            if ((randomGetNext() % 5) == 0)
            {
                templatevtx.v.cn[0] = (templatevtx.v.cn[1] = 255 - (randomGetNext() % 100));
            }
        }
        else
        {
            radius = 30.0f;
        }

        pos.f[0] = flash->from.f[0];
        pos.f[1] = flash->from.f[1];
        pos.f[2] = flash->from.f[2];

        if (startoffset > 0.0f)
        {
            pos.f[0] += startoffset * flash->dir.f[0];
            pos.f[1] += startoffset * flash->dir.f[1];
            pos.f[2] += startoffset * flash->dir.f[2];
        }
        else
        {
            dist += startoffset;
            startoffset = 0.0f;
        }

        if (flash->maxdist < (startoffset + dist))
        {
            dist = flash->maxdist - startoffset;
        }

        right.f[0] = (flash->dir.f[1] * (playerpos->f[2] - (pos.f[2] + (dist * flash->dir.f[2])))) - (flash->dir.f[2] * (playerpos->f[1] - (pos.f[1] + (dist * flash->dir.f[1]))));
        right.f[1] = (flash->dir.f[2] * (playerpos->f[0] - (pos.f[0] + (dist * flash->dir.f[0])))) - (flash->dir.f[0] * (playerpos->f[2] - (pos.f[2] + (dist * flash->dir.f[2]))));
        right.f[2] = (flash->dir.f[0] * (playerpos->f[1] - (pos.f[1] + (dist * flash->dir.f[1])))) - (flash->dir.f[1] * (playerpos->f[0] - (pos.f[0] + (dist * flash->dir.f[0]))));

        if (((right.f[0] != 0.0f) || (right.f[1] != 0.0f)) || (right.f[2] != 0.0f))
        {
            guNormalize(&right.f[0], &right.f[1], &right.f[2]);
            right.f[0] *= radius;
            right.f[1] *= radius;
            right.f[2] *= radius;
        }
        else
        {
            right.f[0] = 0.0f;
            right.f[1] = radius;
            right.f[2] = 0.0f;
        }

        up.f[0] = (flash->dir.f[1] * right.f[2]) - (flash->dir.f[2] * right.f[1]);
        up.f[1] = (flash->dir.f[2] * right.f[0]) - (flash->dir.f[0] * right.f[2]);
        up.f[2] = (flash->dir.f[0] * right.f[1]) - (flash->dir.f[1] * right.f[0]);

        guNormalize(&up.f[0], &up.f[1], &up.f[2]);

        up.f[0] *= radius;
        up.f[1] *= radius;
        up.f[2] *= radius;

        if (flash->weaponnum == ITEM_LASER)
        {
            vtx = dynAllocateVertices(8);
        }
        else
        {
            vtx = dynAllocateVertices(4);
        }

        mtx = dynAllocateMatrix();

        mtx4LoadTranslation(&pos, &mtxf);
        mtx4ScaleRows012(0.1f, &mtxf);
        mtx4MultAffineInPlace(worldtoscreen, &mtxf);
        mtxF2L(&mtxf, (Mtxf *)mtx);

        vtx[0] = templatevtx;
        vtx[1] = templatevtx;
        vtx[2] = templatevtx;
        vtx[3] = templatevtx;

        if (flash->weaponnum == ITEM_LASER)
        {
            vtx[4] = templatevtx;
            vtx[5] = templatevtx;
            vtx[6] = templatevtx;
            vtx[7] = templatevtx;
        }

        if (flash->weaponnum == ITEM_WATCHLASER)
        {
            tmp.f[0] = pos.f[0] + flash->dir.f[0] * dist;
            tmp.f[1] = pos.f[1] + flash->dir.f[1] * dist;
            tmp.f[2] = pos.f[2] + flash->dir.f[2] * dist;

            mtx4TransformVecInPlace(worldtoscreen, &tmp);

            divin[0] = divin[1] = radius / 10.0f;

            dist_to_start = -tmp.f[2];
            camViewDeltaToScreen((coord3d *)divin, dist_to_start, &divout);

            if (divout.f[0] < 2.0f)
            {
                tmp.f[0] *= divout.f[0] * 0.5f;
                tmp.f[1] *= divout.f[0] * 0.5f;
                tmp.f[2] *= divout.f[0] * 0.5f;
            }

            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &tmp);

            tmp.f[0] -= pos.f[0];
            tmp.f[1] -= pos.f[1];
            tmp.f[2] -= pos.f[2];

            flareoffset.x = tmp.f[0] * 10.0f;
            flareoffset.y = tmp.f[1] * 10.0f;
            flareoffset.z = tmp.f[2] * 10.0f;
        }
        else
        {
            flareoffset.x = flash->dir.f[0] * (dist * 10.0f);
            flareoffset.y = flash->dir.f[1] * (dist * 10.0f);
            flareoffset.z = flash->dir.f[2] * (dist * 10.0f);
        }

        vtx[0].v.ob[0] = right.f[0];
        vtx[0].v.ob[1] = right.f[1];
        vtx[0].v.ob[2] = right.f[2];
        vtx[0].v.tc[0] = image->width << 5;
        vtx[0].v.tc[1] = 0;

        vtx[1].v.ob[0] = -right.f[0];
        vtx[1].v.ob[1] = -right.f[1];
        vtx[1].v.ob[2] = -right.f[2];
        vtx[1].v.tc[0] = 0;
        vtx[1].v.tc[1] = 0;

        vtx[2].v.ob[0] = flareoffset.f[0] + (right.f[0] * 0.9f);
        vtx[2].v.ob[1] = flareoffset.f[1] + (right.f[1] * 0.9f);
        vtx[2].v.ob[2] = flareoffset.f[2] + (right.f[2] * 0.9f);
        vtx[2].v.tc[0] = image->width << 5;
        vtx[2].v.tc[1] = image->height << 5;

        vtx[3].v.ob[0] = flareoffset.f[0] - (right.f[0] * 0.9f);
        vtx[3].v.ob[1] = flareoffset.f[1] - (right.f[1] * 0.9f);
        vtx[3].v.ob[2] = flareoffset.f[2] - (right.f[2] * 0.9f);
        vtx[3].v.tc[0] = 0;
        vtx[3].v.tc[1] = image->height << 5;

        if (flash->weaponnum == ITEM_LASER)
        {
            f14 = playerpos->f[0] - pos.f[0];
            f16 = playerpos->f[1] - pos.f[1];
            f18 = playerpos->f[2] - pos.f[2];

            dist_to_start = (f14 * f14) + (f16 * f16) + (f18 * f18);

            f14 = playerpos->f[0] - (pos.f[0] + (flash->dir.f[0] * dist));
            f16 = playerpos->f[1] - (pos.f[1] + (flash->dir.f[1] * dist));
            f18 = playerpos->f[2] - (pos.f[2] + (flash->dir.f[2] * dist));

            d = (f14 * f14) + (f16 * f16) + (f18 * f18);

            if (d < dist_to_start)
            {
                extraorigin.f[0] = flareoffset.f[0];
                extraorigin.f[1] = flareoffset.f[1];
                extraorigin.f[2] = flareoffset.f[2];
                extra_scale *= 0.9f;
            }

            vtx[4].v.ob[0] = extraorigin.f[0] + (up.f[0] * extra_scale);
            vtx[4].v.ob[1] = extraorigin.f[1] + (up.f[1] * extra_scale);
            vtx[4].v.ob[2] = extraorigin.f[2] + (up.f[2] * extra_scale);
            vtx[4].v.tc[0] = g_ImageFlare5->width << 5;
            vtx[4].v.tc[1] = g_ImageFlare5->height << 5;

            vtx[5].v.ob[0] = extraorigin.f[0] - (up.f[0] * extra_scale);
            vtx[5].v.ob[1] = extraorigin.f[1] - (up.f[1] * extra_scale);
            vtx[5].v.ob[2] = extraorigin.f[2] - (up.f[2] * extra_scale);
            vtx[5].v.tc[0] = 0;
            vtx[5].v.tc[1] = 0;

            vtx[6].v.ob[0] = extraorigin.f[0] + (right.f[0] * extra_scale);
            vtx[6].v.ob[1] = extraorigin.f[1] + (right.f[1] * extra_scale);
            vtx[6].v.ob[2] = extraorigin.f[2] + (right.f[2] * extra_scale);
            vtx[6].v.tc[0] = 0;
            vtx[6].v.tc[1] = g_ImageFlare5->height << 5;

            vtx[7].v.ob[0] = extraorigin.f[0] - (right.f[0] * extra_scale);
            vtx[7].v.ob[1] = extraorigin.f[1] - (right.f[1] * extra_scale);
            vtx[7].v.ob[2] = extraorigin.f[2] - (right.f[2] * extra_scale);
            vtx[7].v.tc[0] = g_ImageFlare5->width << 5;
            vtx[7].v.tc[1] = 0;
        }

        gSPClearGeometryMode(gdl++, G_CULL_BACK);

        // gSPMatrix(gdl++, osVirtualToPhysical(mtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH), written out
        {
            cmd = gdl++;
            cmd->words.w0 = _SHIFTL(G_MTX, 24, 8)
                | _SHIFTL(G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH, 16, 8)
                | _SHIFTL(sizeof(Mtx), 0, 16);
            cmd->words.w1 = osVirtualToPhysical(mtx);
        }

        if (flash->weaponnum == ITEM_LASER)
        {
            texSelect(&gdl, g_ImageFlare5, TEXBLEND_CLOUD, zbufferMode, 2);

            gSPVertex(gdl++, osVirtualToPhysical(vtx), 8, 0);

            gSP4Triangles(gdl++, 4, 5, 6, 4, 5, 7, 0, 0, 0, 0, 0, 0);

            texSelect(&gdl, image, TEXBLEND_CLOUD, zbufferMode, 2);

            gSP4Triangles(gdl++, 0, 2, 3, 0, 3, 1, 0, 0, 0, 0, 0, 0);
        }
        else
        {
            texSelect(&gdl, image, TEXBLEND_CLOUD, zbufferMode, 2);

            gSPVertex(gdl++, osVirtualToPhysical(vtx), 4, 0);

            gSP4Triangles(gdl++, 0, 2, 3, 0, 3, 1, 0, 0, 0, 0, 0, 0);
        }
    }

    return gdl;
}


/**
 * Moves a beam along its path, and ends it once it reaches the end.
 */
void beamTick(BeamRecord* beam)
{
    if (beam->age >= 0)
    {
        if (g_ClockTimer < 3)
        {
#ifdef VERSION_US
            beam->dist += beam->speed * g_GlobalTimerDelta;
#else
            beam->dist += beam->speed * g_GlobalTimerDeltaRaw;
#endif
        }
        else
        {
            beam->dist += beam->speed * (2.0f + ((f32) randomGetNext() * 2.3283064e-10f * 0.5f));
        }

        if (beam->maxdist <= beam->dist)
        {
            beam->age = -1;
            return;
        }

        beam->age++;
    }
}
