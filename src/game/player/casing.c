/**
 * Ejected bullet casings: spawning them when a gun fires (casingCreate,
 * gunEjectCasings), moving and bouncing them each frame, and drawing them.
 * See docs/subsystems/weapons.md.
 */
#include <music.h>
#include "assets/obseg/text/LgunE.h"
#include "game/gfx/dyn.h"
#include "game/level/lv.h"
#include "game/menu/watchmenu.h"
#include "game/model/model.h"
#include "game/model/modelfile.h"
#include "game/player/bondview.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "random.h"
#include "game/player/casing.h"

CasingRecord g_Casings[20];

// The casing landing sound; only one plays at a time
static ALSoundState *g_CasingSfxState;

static ModelRenderData g_DefaultCasingModelRenderData = {.zbufferenabled = TRUE, .flags = 3};

// How ejected casings fly: speeds, spin and gravity (pistol and rifle rounds)
static const f32 g_CasingSwitchScale = 0.10000001f;
static const f32 g_PistolCasingHorizontalSpeed = 0.5333333f;
static const f32 g_PistolCasingRotationScaleX = M_TAU_F;
static const f32 g_PistolCasingRotationOffsetX = M_PI_F / 8.0f;
static const f32 g_PistolCasingRotationScaleY = M_TAU_F;
static const f32 g_PistolCasingRotationOffsetY = M_PI_F / 8.0f;
static const f32 g_PistolCasingRotationScaleZ = M_TAU_F;
static const f32 g_PistolCasingRotationOffsetZ = M_PI_F / 8.0f;
#if VERSION_EU
static const f32 g_PistolCasingRandomDivisor = 931050.0f;
#else
static const f32 g_PistolCasingRandomDivisor = 775875.0f;
#endif
static const f32 g_PistolCasingGravity = 0.2777778f;
static const f32 g_RifleCasingHorizontalSpeed = 1.4166666f;
static const f32 g_RifleCasingVerticalSpeed = 1.6666666f;
static const f32 g_RifleCasingRotationScaleX = M_TAU_F;
static const f32 g_RifleCasingRotationOffsetX = M_PI_F / 8.0f;
static const f32 g_RifleCasingRotationScaleY = M_TAU_F;
static const f32 g_RifleCasingRotationOffsetY = M_PI_F / 8.0f;
static const f32 g_RifleCasingRotationScaleZ = M_TAU_F;
static const f32 g_RifleCasingRotationOffsetZ = M_PI_F / 8.0f;
#if VERSION_EU
static const f32 g_RifleCasingRandomDivisor = 931050.0f;
#else
static const f32 g_RifleCasingRandomDivisor = 775875.0f;
#endif
static const f32 g_RifleCasingGravity = 0.2777778f;
static const f32 g_CasingGravity = 0.2777778f;
static const f32 g_CasingModelScale = 0.10000001f;
static const f32 g_CasingMinMatrixTranslation = -30000.0f;
static const f32 g_CasingMaxMatrixTranslation = 30000.0f;


/**
 * Spawns a casing using the model and the gun's matrix, or returns NULL if
 * all slots are in use.
 */
CasingRecord* casingCreate(ModelFileHeader* header, Mtxf* mtx)
{
    CasingRecord* entry = g_Casings;
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);

    while (entry < end && entry->header != NULL)
    {
        entry++;
    }

    if (entry < end)
    {
        entry->header = header;

        entry->pos.x = mtx->m[3][0];
        entry->pos.y = mtx->m[3][1];
        entry->pos.z = mtx->m[3][2];
#if VERSION_EU
        mtx4ToMtx3(mtx, &entry->rot_mtx);
#else
        entry->rot_mtx.m[0][0] = mtx->m[0][0];
        entry->rot_mtx.m[0][1] = mtx->m[0][1];
        entry->rot_mtx.m[0][2] = mtx->m[0][2];
        entry->rot_mtx.m[0][3] = 0.0f;

        entry->rot_mtx.m[1][0] = mtx->m[1][0];
        entry->rot_mtx.m[1][1] = mtx->m[1][1];
        entry->rot_mtx.m[1][2] = mtx->m[1][2];
        entry->rot_mtx.m[1][3] = 0.0f;

        entry->rot_mtx.m[2][0] = mtx->m[2][0];
        entry->rot_mtx.m[2][1] = mtx->m[2][1];
        entry->rot_mtx.m[2][2] = mtx->m[2][2];
        entry->rot_mtx.m[2][3] = 0.0f;

        entry->rot_mtx.m[3][0] = 0.0f;
        entry->rot_mtx.m[3][1] = 0.0f;
        entry->rot_mtx.m[3][2] = 0.0f;
        entry->rot_mtx.m[3][3] = 1.0f;
#endif
        return entry;
    }

    return NULL;
}


 
 
 
/**
 * Ejects a spent cartridge casing from the gun in the given hand.
 */
void gunEjectCasings(GUNHAND handnum, f32 floor_y_pos)
{
    CasingRecord *casing;
    Mtxf mtx;
    ITEM_IDS weaponid;
    coord3d *switchdata;
    ModelFileHeader *cartridge_header;
    coord3d switchpos;
    ModelNode *switch0;
    coord3d rot;
#if VERSION_EU
    Mtxf rotmtx;
#endif
    f32 rand;
#ifndef VERSION_EU
    s32 limit;
#endif
    f32 frac = 0.0f;
#if VERSION_EU
    s32 randlimit;
#endif
    f32 oldvely;
    f32 newvely;
#ifndef VERSION_EU
    s32 randlimit;
#endif
    struct hand *hand;
    u32 randval;
 
    weaponid = gunGetWeapon(handnum);
    cartridge_header = weaponGetStats(weaponid)->ptr_cartridge_struct;
 
    // Do not create ejected casings in multiplayer.
    if ((cartridge_header == NULL) || (getPlayerCount() >= 2))
    {
        return;
    }
 
    hand = &g_CurrentPlayer->hands[handnum];
    switch0 = g_CurrentPlayer->handModelHeader[handnum].Switches[0];
 
    if (switch0 != NULL)
    {
        switchdata = (coord3d *) switch0->Data;
 
        switchpos.x = switchdata->x * g_CasingSwitchScale;
        switchpos.y = switchdata->y * g_CasingSwitchScale;
        switchpos.z = switchdata->z * g_CasingSwitchScale;
 
        mtx4LoadTranslation(&switchpos, &mtx);
        mtx4MultInPlace(&hand->gunWorldMtx, &mtx);
    }
    else
    {
        mtx4Copy(&hand->gunWorldMtx, &mtx);
    }
 
    casing = casingCreate(cartridge_header, &mtx);
 
    if (casing == NULL)
    {
        return;
    }
 
    rot = (coord3d){0};
    casing->floor_y_pos = floor_y_pos;
 
    if (((((weaponid == ITEM_WPPK) || (weaponid == ITEM_WPPKSIL)) || (weaponid == ITEM_TT33)) || (weaponid == ITEM_SILVERWPPK)) || (weaponid == ITEM_GOLDWPPK))
    {
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        newvely = 0.0625f;
        casing->vel.x = -(((rand * g_PistolCasingHorizontalSpeed) * newvely) + g_PistolCasingHorizontalSpeed);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.y = ((rand * 2.5f) * 0.0625f) + 2.5f;
        casing->vel.z = frac * 0.0f;
 
        mtx4RotateVecInPlace(&hand->gunWorldMtx, &casing->vel);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.x = (((rand + rand) * g_PistolCasingRotationScaleX) * newvely) - g_PistolCasingRotationOffsetX;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.y = (((rand + rand) * g_PistolCasingRotationScaleY) * newvely) - g_PistolCasingRotationOffsetY;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.z = (((rand + rand) * g_PistolCasingRotationScaleZ) * newvely) - g_PistolCasingRotationOffsetZ;
 
#if VERSION_EU
        mtx4LoadRotation(&rot, &rotmtx);
        mtx4ToMtx3(&rotmtx, &casing->rot_velocity_mtx);
#else
        mtx4LoadRotation(&rot, &casing->rot_velocity_mtx);
#endif
 
#if VERSION_EU
        randlimit = ((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10;
        randlimit = randlimit + 0x158679;
        randval = randomGetNext();
        oldvely = casing->vel.y;
        frac = ((f32) ((u32) (randval % randlimit))) / g_PistolCasingRandomDivisor;
#else
        randlimit = (((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10) + 0x158679;
        limit = randlimit;
        randval = randomGetNext();
        oldvely = casing->vel.y;
        frac = ((f32) ((u32) (randval % limit))) / g_PistolCasingRandomDivisor;
#endif
        newvely = oldvely - (frac * g_PistolCasingGravity);
 
        casing->vel.y = newvely;
        casing->pos.y += (frac * (oldvely + newvely)) * 0.5f;
        casing->pos.x += frac * casing->vel.x;
        casing->pos.z += frac * casing->vel.z;
 
        if (g_ClockTimer >= 1)
        {
            casing->vel.x += (hand->gunWorldMtx.m[3][0] - hand->prevGunWorldMtx.m[3][0]) / g_GlobalTimerDelta;
            casing->vel.y += (hand->gunWorldMtx.m[3][1] - hand->prevGunWorldMtx.m[3][1]) / g_GlobalTimerDelta;
            casing->vel.z += (hand->gunWorldMtx.m[3][2] - hand->prevGunWorldMtx.m[3][2]) / g_GlobalTimerDelta;
        }
    }
    else
    {
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.x = -(((rand * g_RifleCasingHorizontalSpeed) * 0.125f) + g_RifleCasingHorizontalSpeed);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        casing->vel.y = ((rand * g_RifleCasingVerticalSpeed) * 0.125f) + g_RifleCasingVerticalSpeed;
        casing->vel.z = 0.0f;
 
        mtx4RotateVecInPlace(&hand->gunWorldMtx, &casing->vel);
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.x = (((rand + rand) * g_RifleCasingRotationScaleX) * 0.0625f) - g_RifleCasingRotationOffsetX;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.y = (((rand + rand) * g_RifleCasingRotationScaleY) * 0.0625f) - g_RifleCasingRotationOffsetY;
 
        rand = ((f32) ((u32) randomGetNext())) * 2.3283064e-10f;
        rot.z = (((rand + rand) * g_RifleCasingRotationScaleZ) * 0.0625f) - g_RifleCasingRotationOffsetZ;
 
#if VERSION_EU
        mtx4LoadRotation(&rot, &rotmtx);
        mtx4ToMtx3(&rotmtx, &casing->rot_velocity_mtx);
#else
        mtx4LoadRotation(&rot, &casing->rot_velocity_mtx);
#endif
        randval = ((s32) ((randomGetNext() >> 24) * 0x158679)) >> 10;
#if VERSION_EU
        randval = randval + 0x158679;
        randlimit = randomGetNext();
        oldvely = (&casing->vel)->y;
        frac = ((f32) ((u32) (randlimit % randval))) / g_RifleCasingRandomDivisor;
#else
        randlimit = randval + 0x158679;
        randval = randomGetNext();
        oldvely = (&casing->vel)->y;
        frac = ((f32) ((u32) (randval % randlimit))) / g_RifleCasingRandomDivisor;
#endif
        newvely = (casing->vel.y = oldvely - (frac * g_RifleCasingGravity));
 
        casing->pos.y += (frac * (oldvely + newvely)) * 0.5f;
        casing->pos.x += frac * casing->vel.x;
        casing->pos.z += frac * casing->vel.z;
 
        if (g_ClockTimer > 0)
        {
            casing->vel.x += (hand->gunWorldMtx.m[3][0] - hand->prevGunWorldMtx.m[3][0]) / g_GlobalTimerDelta;
            casing->vel.y += (hand->gunWorldMtx.m[3][1] - hand->prevGunWorldMtx.m[3][1]) / g_GlobalTimerDelta;
            casing->vel.z += (hand->gunWorldMtx.m[3][2] - hand->prevGunWorldMtx.m[3][2]) / g_GlobalTimerDelta;
        }
    }
 
}


/**
 * Moves one casing with gravity and spin. When it reaches the floor it plays
 * the casing sound and is removed.
 */
void casingTick(CasingRecord* casing)
{
    f32 new_val_y;
    f32 delta;
    s32 i;

    delta = g_GlobalTimerDelta;
    new_val_y = casing->vel.y - (delta * g_CasingGravity);

    casing->pos.y += delta * 0.5f * (casing->vel.y + new_val_y);

    if (casing->pos.y < casing->floor_y_pos)
    {
        if (g_CasingSfxState == 0 && (g_ClockTimer > 0))
        {
            if ((g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_FIRE) && (g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_FIRE))
            {
                // Play bullet casing rolling on floor sound
                sndPlaySfx(g_musicSfxBufferPtr, CART_SPENT_SFX, &g_CasingSfxState);
            }
        }

        // This casing is removed and not updated anymore
        casing->header = NULL;
        return;
    }

    casing->vel.y = new_val_y;
    casing->pos.x += delta * casing->vel.x;
    casing->pos.z += delta * casing->vel.z;

    for (i = 0; i < g_ClockTimer; i++)
    {
#if defined(VERSION_US)
        mtx4MultAffineInPlace(&casing->rot_velocity_mtx, &casing->rot_mtx);
#else
        mtx3MultInPlace(&casing->rot_velocity_mtx, &casing->rot_mtx);
#endif
    }
}


/**
 * Moves every live casing.
 */
void casingsTick(void)
{
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);
    CasingRecord* entry = g_Casings;

    while (entry < end)
    {
        if (entry->header)
        {
            casingTick(entry);
        }

        entry++;
    }
}


/**
 * Draws one casing.
 */
void casingRender(CasingRecord *casing, Gfx **gdl)
{
    Gfx             *savedgdl = *gdl;
    ModelFileHeader *model_header = casing->header;
    RenderPosView   *model_matrices = dynAllocate(model_header->numMatrices * sizeof(RenderPosView));
    Model            model = {};
    ModelRenderData  render_data = g_DefaultCasingModelRenderData;
    Mtxf             casing_model_mtx;
    s32              axis_offset;
    s32              matrix_translation_in_range = TRUE;
    f32              max_matrix_translation;
    f32              model_scale_or_min_translation;
    u8              *matrix_axis_ptr;

    modelCalculateRwDataLen(model_header);
    modelInit(&model, model_header, NULL);

    model.render_pos = model_matrices;

#if defined(VERSION_EU)
    mtx3ToMtx4(&casing->rot_mtx, &casing_model_mtx);
#else
    mtx4Copy(&casing->rot_mtx, &casing_model_mtx);
#endif

    model_scale_or_min_translation = g_CasingModelScale;
    mtx4ScaleRows012(model_scale_or_min_translation, &casing_model_mtx);

    mtx4SetTranslation(&casing->pos, &casing_model_mtx);

    mtx4MultAffine(
        camGetWorldToScreenMtxf(),
        &casing_model_mtx,
        (Mtxf *)model.render_pos);

    model_scale_or_min_translation = g_CasingMinMatrixTranslation;
    max_matrix_translation         = g_CasingMaxMatrixTranslation;

    axis_offset     = 0;
    matrix_axis_ptr = (u8 *)model.render_pos;

    // Offset 0x30 is m[3][0]; advancing the pointer checks translation X, Y and Z.
    while (axis_offset != 12)
    {
        if (max_matrix_translation < *(f32 *)(matrix_axis_ptr + 0x30))
        {
            matrix_translation_in_range = FALSE;
        }
        else if (*(f32 *)(matrix_axis_ptr + 0x30) < model_scale_or_min_translation)
        {
            matrix_translation_in_range = FALSE;
        }

        axis_offset += 4;
        matrix_axis_ptr += 4;
    }

    if (matrix_translation_in_range)
    {
        render_data.zbufferenabled = 0;
        render_data.gdl            = savedgdl;
        render_data.mtxlist        = (Mtxf *)model_matrices;
        render_data.PropType       = PROP_TYPE_WEAPON;

        render_data.envcolour.word =
            ((g_CurrentPlayer->tileColor.a |
              (g_CurrentPlayer->tileColor.r << 24)) |
             (g_CurrentPlayer->tileColor.g << 16)) |
            (g_CurrentPlayer->tileColor.b << 8);

        modelRender(&render_data, &model);

        *gdl = render_data.gdl;

        mtxsF2L(model_matrices, model_header->numMatrices);
    }
}


/**
 * Draws every live casing.
 */
void casingsRender(Gfx **gdl)
{
    CasingRecord* end = g_Casings + ARRAYCOUNT(g_Casings);
    CasingRecord* entry = g_Casings;

    while (entry < end)
    {
        if (entry->header)
        {
            casingRender(entry, gdl);
        }
        
        entry++;
    }
}


/**
 * Level start: clears the hit sound slots and loads the casing models.
 */
void casingsLoadModels(void) 
{
    s32 i = 0;

    g_CasingSfxState = NULL;

    while (i < NUM_IMPACT_SFX_STATES)
    {
        g_ImpactSfxStates[i] = NULL;
        i++;
    }

    i = 0;
    g_GunDebApplyKeyframe = 0;
    g_gunDebKeyframeIndex = 0;

    while (ejected_cartridge[i].header != 0) 
    {
        modelFileLoadToPool(ejected_cartridge[i].header, ejected_cartridge[i].text);
        i++;
    }
}
