/**
 * The hands each frame: the hand state machine (gunTickHandState), firing
 * (gunUpdateAndFire), placing and drawing the gun models, reloading, the noise
 * a gun makes, the crosshair and aim. Shots, casings, beams, ammo, the ammo HUD
 * and stats are in their own files. See docs/subsystems/weapons.md.
 */
#include <ultra64.h>
#include <assert.h>
#include <bondaicommands.h>
#include "game/audio/sfxvolume.h"
#include "game/world/bg.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/prop/explosion.h"
#include "game/hud/healthbar.h"
#include "game/hud/sparks.h"
#include "game/save/file.h"
#include "game/world/bgfog.h"
#include "include/limits.h"
#include <bondconstants.h>
#include <bondtypes.h>
#include <bondgame.h>
#include <music.h>
#include <snd.h>
#include "game/player/bondview.h"
#include "game/player/bondinv.h"
#include "game/player/gun.h"
#include "game/model/chrobjdata.h"
#include "game/prop/propobj.h"
#include "game/mission/objective.h"
#include "game/math/quaternion.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texrect.h"
#include "game/gfx/texcmds.h"
#include "game/world/surface.h"
#include "game/player/player.h"
#include "game/level/lv.h"
#include "random.h"
#include "game/math/trig.h"
#include "game/prop/setup.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/gfx/texload.h"
#include "fr.h"
#include "assets/obseg/text/LgunE.h"
#include "game/text/text.h"
#include "game/chr/chrai.h"
#include "game/model/model.h"
#include "game/menu/watchmenu.h"
#include "game/menu/mpmenu.h"
#include "joy.h"
#include "game/math/matrix.h"
#include "game/player/bondinv.h"
#include "game/nav/stan.h"
#include "gbi_extension.h"
#include "game/math/interp.h"
#include "bondtypes.h"
#include "game/gfx/dyn.h"
#include "game/prop/prop.h"
#include "game/text/language.h"
#include "game/world/lightfixture.h"
#include "game/prop/monitor.h"



#ifdef REFRESH_PAL
#define THROWN_ITEM_REFRESH_RATE 50
#define DUAL_WIELD_TRIGGER_SWAP_TICKS 24
#define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS 36
#define WATCH_SOUND_DURATION_TICKS 250
#define GUN_SPRING_DAMP 0.9402999877929688f
#define GUN_SPRING_SCALE 0.05970001220703125f
#else
#define THROWN_ITEM_REFRESH_RATE 60
#define DUAL_WIELD_TRIGGER_SWAP_TICKS 20
#define DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS 30
#define WATCH_SOUND_DURATION_TICKS 300
#define GUN_SPRING_DAMP 0.95f
#define GUN_SPRING_SCALE 0.050000012f
#endif

s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand);
extern GunModelFileRecord gitem_structs[];
extern u16 watchlaser_fire_sounds[2];
extern u16 knife_throw_sounds[3];
extern Weapon1PTransformKeyframe knifeSlashKeyframes1[];
extern Weapon1PTransformKeyframe knifeSlashKeyframes2[];
extern Weapon1PTransformKeyframe throwKnifeDrawBackKeyframes[];
extern Weapon1PTransformKeyframe throwKnifeReleaseKeyframes[];
extern Weapon1PTransformKeyframe grenadeThrowKeyframes[];
extern Weapon1PTransformKeyframe timedMineThrowKeyframes[];
extern Weapon1PTransformKeyframe proxMineThrowKeyframes[];
extern Weapon1PTransformKeyframe remoteMineThrowKeyframes[];
extern Weapon1PTransformKeyframe fistMeleeKeyframes1[];
extern Weapon1PTransformKeyframe fistMeleeKeyframes2[];
extern Weapon1PTransformKeyframe sniperMeleeKeyframes1[];
extern Weapon1PTransformKeyframe sniperMeleeKeyframes2[];
extern Weapon1PTransformKeyframe taserFireKeyFrames[];
extern Weapon1PTransformKeyframe taserRaiseKeyframes[];
extern struct ModelSkeleton skeleton_gun_kf7;



void gunUpdateAndFire(GUNHAND handnum)
{
    Mtxf *rwmtx;
    Mtxf gunmtx;
    Mtxf flashmtx;
    Mtxf flash2mtx;
    Mtxf tmpmtx;
    ModelFileHeader *mdlhdr;
    coord3d gunofs = {{0.0f, 0.0f, 0.0f}};
    Mtxf rotmtx;
    Mtxf aimmtx;
    struct hand *hand;
    s32 *flashvisptr;
    f32 *flashdata;
    ModelNode *node;
    s32 j;
    ITEM_IDS item;
    WeaponStats *itemstats;
    f32 rndf;
    u32 rnd;
    f32 stackpad_scale;
    coord3d blendedpos;
    coord3d blendedlook;
    coord3d blendedup;
    s32 i;
    coord3d trigrot;
    coord3d taserrot;
    coord3d fistrot;
    f32 *cylinderdata;
    ModelNode **hammerdata;
    Model *model;
    coord3d flashpos;
    f32 flashscale;
    f32 flashext;
    f32 *nodeptr;
    f32 *nodepos;
    f32 *swdata;
    s32 sw6mtxidx;
    f32 *hingedata;
    f32 *sw7data;
    s32 sw7mtxidx;
    s32 shellidx;

    flashvisptr = NULL;
    flashdata = NULL;
    hand = &g_CurrentPlayer->hands[handnum];
    item = gunGetShownWeapon(handnum);
    itemstats = weaponGetStats(item);

    /**
     * When switching from a single weapon to dual wielding, both gun models interpolate a little to the sides of the screen
     * over a period of 4 seconds. And when switching back to a single weapon that weapon moves back more towards the
     * center of the screen.
     */
    if (handnum == GUNRIGHT)
    {
        if (weaponHasFlag(gunGetShownWeapon(1), WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) != 0)
        {
            hand->dualOffset += (2.0f * g_GlobalTimerDelta) / 240.0f;

            if (hand->dualOffset > 2.0f)
            {
                hand->dualOffset = 2.0f;
            }
        }
        else
        {
            hand->dualOffset -= (2.0f * g_GlobalTimerDelta) / 240.0f;

            if (hand->dualOffset < 0.0f)
            {
                hand->dualOffset = 0.0f;
            }
        }
    }
    else if (weaponHasFlag(gunGetShownWeapon(0), WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) != 0)
    {
        hand->dualOffset -= (2.0f * g_GlobalTimerDelta) / 240.0f;

        if (hand->dualOffset < (-2.0f))
        {
            hand->dualOffset = -2.0f;
        }
    }
    else
    {
        hand->dualOffset += (2.0f * g_GlobalTimerDelta) / 240.0f;

        if (hand->dualOffset > 0.0f)
        {
            hand->dualOffset = 0.0f;
        }
    }

    /**
     * Gun sway system. This moves the held weapons in figure-eight pattern which becomes bigger depending how fast the player is moving.
     */
    blendedpos = (coord3d){0};
    blendedlook = (coord3d){{0.0f, 0.0f, -1.0f}};
    blendedup = (coord3d){{0.0f, 1.0f, 0.0f}};

    i = hand->curblendpos;

    interpCatmullRom3(&hand->blendpos[(i + 3) % 4], &hand->blendpos[i], &hand->blendpos[(i + 1) % 4], &hand->blendpos[(i + 2) % 4], hand->dampt, &blendedpos);
    interpCatmullRom3(&hand->blendlook[(i + 3) % 4], &hand->blendlook[i], &hand->blendlook[(i + 1) % 4], &hand->blendlook[(i + 2) % 4], hand->dampt, &blendedlook);
    interpCatmullRom3(&hand->blendup[(i + 3) % 4], &hand->blendup[i], &hand->blendup[(i + 1) % 4], &hand->blendup[(i + 2) % 4], hand->dampt, &blendedup);

    blendedpos.x *= g_CurrentPlayer->gunposamplitude;
    blendedpos.y *= g_CurrentPlayer->gunposamplitude;
    blendedpos.z *= g_CurrentPlayer->gunposamplitude;
    blendedpos.x += hand->weapon_theta_displacement;
    blendedpos.y += hand->weapon_verta_displacement;
    blendedpos.x += gunGetDualOffset(handnum);

    j = 0;

    if (g_ClockTimer > 0)
    {
        do
        {
            j += 1;
            hand->spring_pos_x = (GUN_SPRING_DAMP * hand->spring_pos_x) + ((f32 *) (&blendedpos))[0];
            hand->spring_pos_y = (GUN_SPRING_DAMP * hand->spring_pos_y) + ((f32 *) (&blendedpos))[1];
            hand->spring_pos_z = (GUN_SPRING_DAMP * hand->spring_pos_z) + ((f32 *) (&blendedpos))[2];
            hand->spring_look_x = (GUN_SPRING_DAMP * hand->spring_look_x) + ((f32 *) (&blendedlook))[0];
            hand->spring_look_y = (GUN_SPRING_DAMP * hand->spring_look_y) + ((f32 *) (&blendedlook))[1];
            hand->spring_look_z = (GUN_SPRING_DAMP * hand->spring_look_z) + ((f32 *) (&blendedlook))[2];
            hand->spring_up_x = (GUN_SPRING_DAMP * hand->spring_up_x) + ((f32 *) (&blendedup))[0];
            hand->spring_up_y = (GUN_SPRING_DAMP * hand->spring_up_y) + ((f32 *) (&blendedup))[1];
            hand->spring_up_z = (GUN_SPRING_DAMP * hand->spring_up_z) + ((f32 *) (&blendedup))[2];
        }
        while (j < g_ClockTimer);
    }

    hand->sway_pos_x = hand->spring_pos_x * GUN_SPRING_SCALE;
    hand->sway_pos_y = hand->spring_pos_y * GUN_SPRING_SCALE;
    hand->sway_pos_z = hand->spring_pos_z * GUN_SPRING_SCALE;
    hand->sway_look_x = hand->spring_look_x * GUN_SPRING_SCALE;
    hand->sway_look_y = hand->spring_look_y * GUN_SPRING_SCALE;
    hand->sway_look_z = hand->spring_look_z * GUN_SPRING_SCALE;
    hand->sway_up_x = hand->spring_up_x * GUN_SPRING_SCALE;
    hand->sway_up_y = hand->spring_up_y * GUN_SPRING_SCALE;
    hand->sway_up_z = hand->spring_up_z * GUN_SPRING_SCALE;

    // Offset the weapon to the right or left side of the screen depending on which hand it's in.
    if (handnum == GUNRIGHT)
    {
        gunofs.x = (gunSetHorizontalOffset(handnum) + hand->sway_pos_x) + (*hand).gunofs2_x;
    }
    else
    {
        gunofs.x = (gunSetHorizontalOffset(handnum) + hand->sway_pos_x) - hand->gunofs2_x;
    }

    gunofs.y = hand->gunofs2_y + (itemstats->PosY + hand->sway_pos_y);
    gunofs.z = hand->gunofs2_z + (itemstats->PosZ + hand->sway_pos_z);

    if (((item == ITEM_ROCKETLAUNCH) || (item == ITEM_TRIGGER)) || (item == ITEM_WATCHLASER))
    {
        gunofs.y += g_CurrentPlayer->ducking_height_offset / (-100.0f);
        gunofs.z += (3.0f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);

        if ((item == ITEM_ROCKETLAUNCH) && (((optionsGetScreenSize() == SCREEN_SIZE_WIDESCREEN) || (optionsGetScreenSize() == SCREEN_SIZE_CINEMA)) || (optionsGetScreenRatio() == SCREEN_RATIO_16_9)))
        {
            gunofs.y -= 3.0f;
        }
    }
    else if (item == ITEM_TASER)
    {
        gunofs.y += (2.5f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
        gunofs.z += (7.5f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
    }
    else
    {
        gunofs.y += (5.0f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
        gunofs.z += (15.0f * g_CurrentPlayer->ducking_height_offset) / (-100.0f);
    }

    if ((hand->weapon_firing_status != 0) && (weaponHasFlag(item, WEAPONSTATBITFLAG_00000020) != 0))
    {
        if (weaponHasFlag(item, WEAPONSTATBITFLAG_00000040) != 0)
        {
            rnd = randomGetNext();
            rndf = (f32) ((u32) rnd);
            gunofs.x += 0.3f - ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.6f);
        }

        rnd = randomGetNext();
        rndf = (f32) ((u32) rnd);
        gunofs.y += 0.3f - ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.6f);
        rnd = randomGetNext();
        rndf = (f32) ((u32) rnd);
        gunofs.z += 0.3f - ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.6f);
    }

    gunofs.x += (((g_CurrentPlayer->gunAimScreenPos.x - camGetScreenLeft()) - (camGetScreenWidth() * 0.5f)) * itemstats->PlayZ) / (camGetScreenWidth() * 0.5f);

    if ((g_CurrentPlayer->gunAimScreenPos.y - camGetScreenTop()) > (camGetScreenHeight() * 0.5f))
    {
        gunofs.y -= (((g_CurrentPlayer->gunAimScreenPos.y - camGetScreenTop()) - (camGetScreenHeight() * 0.5f)) * itemstats->PlayY) / (camGetScreenHeight() * 0.5f);
    }
    else
    {
        gunofs.y -= (((g_CurrentPlayer->gunAimScreenPos.y - camGetScreenTop()) - (camGetScreenHeight() * 0.5f)) * itemstats->PlayX) / (camGetScreenHeight() * 0.5f);
    }

    gunDebApplyKeyframe();
    mtx4LoadIdentity(&rotmtx);

    if ((item == ITEM_TRIGGER) || (item == ITEM_WATCHLASER))
    {
            trigrot = (coord3d){{6.2536321f, 6.2592888f, 0.204238f}};
            mtx4LoadRotation(&trigrot, &tmpmtx);
            mtx4MultAffineInPlace(&tmpmtx, &rotmtx);
    }
    else if (item == ITEM_TASER)
    {
        taserrot = (coord3d){{0.25044999f, 0.90482301f, 0.28716999f}};
        mtx4LoadRotation(&taserrot, &tmpmtx);
        mtx4MultAffineInPlace(&tmpmtx, &rotmtx);
    }
    else if ((item == ITEM_FIST) && (g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE))
    {
        fistrot = (coord3d){{1.715736f, 0.37460899f, 0.92193699f}};
        mtx4LoadRotation(&fistrot, &tmpmtx);
        mtx4MultAffineInPlace(&tmpmtx, &rotmtx);
        gunofs.x += -2.5f;
        gunofs.y += 27.8f;
        gunofs.z += 2.0f;
    }

    if (hand->useAnimMtx != 0)
    {
        gunofs.x += hand->animMtx.m[3][0];
        gunofs.y += hand->animMtx.m[3][1];
        gunofs.z += hand->animMtx.m[3][2];
        mtx4MultAffineInPlace(&hand->animMtx, &rotmtx);
        rotmtx.m[3][0] = 0.0f;
        rotmtx.m[3][1] = 0.0f;
        rotmtx.m[3][2] = 0.0f;
    }
    else
    {
        hand->recoilAngle = 0.0f;
        hand->recoilPos.x = (-0.0f) + 0.0f;
        hand->recoilPos.y = 0.0f;
        hand->recoilPos.z = (-0.0f) + 0.0f;
    }

    mtx4LoadFacing(&tmpmtx, 0.0f, 0.0f, 0.0f, hand->sway_look_x, hand->sway_look_y, hand->sway_look_z, hand->sway_up_x, hand->sway_up_y, hand->sway_up_z);
    mtx4MultAffineInPlace(&tmpmtx, &rotmtx);
    mtx4LoadAlign(&tmpmtx, 0.0f, gunofs.x - hand->aimPos.x, gunofs.y - hand->aimPos.y, gunofs.z - hand->aimPos.z);
    mtx4MultAffineInPlace(&tmpmtx, &rotmtx);
    mtx4Copy(&rotmtx, &gunmtx);
    mtx4SetTranslation(&gunofs, &gunmtx);
    mtx4Copy(&gunmtx, &hand->gunmtx_camspace);
    mtx4Copy(&hand->gunWorldMtx, &hand->prevGunWorldMtx);
    mtx4MultAffine(camGetViewToWorldMtxf(), &hand->gunmtx_camspace, &hand->gunWorldMtx);
    hand->handVisible = 1;

    if (((((((get_ptr_weapon_model_header_line(item) == 0) || (weaponHasFlag(item, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)) || (weaponHasFlag(item, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0)) 
    || (hand->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP)) || (hand->weapon_action_state == GUN_ANIM_STATE_SWITCH_HOLD)) || (gunHandModelIsEmpty(handnum) == 0)) || (gunGetHandModelItem(handnum) == 0))
    {
        hand->handVisible = 0;
    }

    if ((hand->weapon_ammo_in_magazine <= 0) && (weaponHasFlag(item, WEAPONSTATBITFLAG_SINGLE_USE_RELOAD) != 0))
    {
        hand->handVisible = 0;
    }

    if (hand->handVisible != 0)
    {
        mdlhdr = &g_CurrentPlayer->handModelHeader[handnum];
        rwmtx = (Mtxf *) dynAllocate(mdlhdr->numMatrices * ((s32) (sizeof(Mtxf))));
        j = 0;

        if (mdlhdr->numMatrices > 0)
        {
            do
            {
                mtx4LoadIdentity((Mtxf *) (((u8 *) rwmtx) + (j * ((s32) (sizeof(Mtxf))))));
                j += 1;
            }
            while (j < mdlhdr->numMatrices);
        }

        modelCalculateRwDataLen(mdlhdr);
        model = (Model *) (&hand->gunmodel);


        modelInit(model, mdlhdr, hand->gunmodelRwData);
        gunSetFirstPersonPartsVisible(model, 1);
        gunSetHeldItemVisible(model, hand->heldItemVisible);
        node = mdlhdr->Switches[1];

        if (node != NULL)
        {

            flashvisptr = ((s32 *) (&hand->gunmodelRwData)) + node->Data->Switch.RwDataIndex;
        }

        if (mdlhdr->Switches[3] != NULL)
        {
            flashdata = (f32 *) mdlhdr->Switches[3]->Data;
        }

        hand->gunmodel.render_pos = (RenderPosView *)rwmtx;

        if ((weaponHasFlag(item, WEAPONSTATBITFLAG_MIRROR_DUAL) != 0) && (handnum == GUNLEFT))
        {
            mtx4ScaleRow0(-1.0f, &gunmtx);
        }

        mtx4ScaleRows012(IDO_POINT_ONE, &gunmtx);
        mtx4Copy(&gunmtx, rwmtx);

        if (mdlhdr->Skeleton == (&skeleton_gun_revolver))
        {
            swdata = (f32 *) mdlhdr->Switches[4];

            if (swdata != NULL)
            {
                rndf = 0.0f;
                cylinderdata = (f32 *) ((ModelNode *) swdata)->Data;

                if (item == ITEM_RUGER)
                {
                    if (hand->weapon_action_state == 1)
                    {
#if defined(VERSION_EU)
                        rndf = (((hand->stateTime - (hand->weapon_ammo_in_magazine * 5)) + 0x19) * M_TAU_F) / 30.0f;
#else
                        rndf = (((hand->stateTime - (hand->weapon_ammo_in_magazine * 6)) + 0x1E) * M_TAU_F) / 36.0f;
#endif
                    }
                    else
                    {
                        rndf = ((6 - hand->weapon_ammo_in_magazine) * M_TAU_F) / 6.0f;
                    }
                }
                else if (hand->weapon_action_state == 1)
                {
#if defined(VERSION_EU)
                if (hand->stateTime < 5)
                {
                    rndf = (hand->stateTime * M_TAU_F) / 30.0f;
                }
#else
                if (hand->stateTime < 6)
                {
                    rndf = (hand->stateTime * M_TAU_F) / 36.0f;
                }
#endif
            }

                mtx4LoadZRotation(rndf, &tmpmtx);
                mtx4SetTranslation((coord3d *) cylinderdata, &tmpmtx);
                mtx4Mult(&gunmtx, &tmpmtx, &rwmtx[3]);
            }

            swdata = (f32 *) mdlhdr->Switches[5];

            if (swdata != NULL)
            {
                hammerdata = (ModelNode **) ((ModelNode *) swdata)->Data;

                if (hand->weapon_action_state == 1)
                {
#if defined(VERSION_EU)
                    if (hand->stateTime < 2)
                    {
                        rndf = (2.0f * ((-((f32) hand->stateTime)) * DegToRad(30.0f))) / 5.0f;
                    }
                    else
                    {
                        rndf = (2.0f * ((-((f32) (5 - hand->stateTime))) * DegToRad(30.0f))) / 5.0f;
                    }
#else
                    if (hand->stateTime < 3)
                    {
                        rndf = (2.0f * ((-((f32) hand->stateTime)) * DegToRad(30.0f))) / 6.0f;
                    }
                    else
                    {
                        rndf = (2.0f * ((-((f32) (6 - hand->stateTime))) * DegToRad(30.0f))) / 6.0f;
                    }
#endif
                    mtx4LoadXRotation(rndf, &tmpmtx);
                    mtx4SetTranslation((coord3d *) hammerdata, &tmpmtx);
                }
                else
                {
                    mtx4LoadTranslation((coord3d *) hammerdata, &tmpmtx);
                }

                mtx4Mult(&gunmtx, &tmpmtx, &rwmtx[4]);
            }
        }

        if (flashvisptr != NULL)
        {
            *flashvisptr = 0;
        }

        if (flashdata != NULL)
        {
            rnd = randomGetNext();
            rndf = (f32) ((u32) rnd);
            flashscale = ((rndf * (1.0f / M_U32_MAX_VALUE_F)) * 0.25f) + 1.0f;
            flashext = itemstats->MuzzleFlashExtension;

            if (weaponHasFlag(item, WEAPONSTATBITFLAG_00000001) != 0)
            {
                rnd = randomGetNext();
                rndf = (f32) ((u32) rnd);
                mtx4LoadZRotation((rndf * (1.0f / M_U32_MAX_VALUE_F)) * M_TAU_F, &flashmtx);
                mtx4SetTranslation((coord3d *) flashdata, &flashmtx);
            }
            else
            {
                mtx4LoadTranslation((coord3d *) flashdata, &flashmtx);
            }

            mtx4ScaleRows012(flashscale, &flashmtx);
            mtx4ScaleRow2AndW(flashext, &flashmtx);
            mtx4MultInPlace(&gunmtx, &flashmtx);
            mtx4Copy(&flashmtx, &rwmtx[1]);

            hand->muzzlePos.x = flashmtx.m[3][0];
            hand->muzzlePos.y = flashmtx.m[3][1];
            hand->muzzlePos.z = flashmtx.m[3][2];
            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &hand->muzzlePos);
            hand->muzzleDepth = -flashmtx.m[3][2];

            if (hand->firing != 0)
            {
                if (flashvisptr != NULL)
                {
                    *flashvisptr = 1;
                }

                nodeptr = (f32 *) mdlhdr->Switches[2];

                if (nodeptr != NULL)
                {
                    nodepos = (f32 *) ((ModelNode *) nodeptr)->Data;
                    flashpos.x = (((((f32 *) nodepos)[0] * flashmtx.m[0][0]) + (((f32 *) nodepos)[1] * flashmtx.m[1][0])) + (((f32 *) nodepos)[2] * flashmtx.m[2][0])) + flashmtx.m[3][0];
                    flashpos.y = (((((f32 *) nodepos)[0] * flashmtx.m[0][1]) + (((f32 *) nodepos)[1] * flashmtx.m[1][1])) + (((f32 *) nodepos)[2] * flashmtx.m[2][1])) + flashmtx.m[3][1];
                    flashpos.z = (((((f32 *) nodepos)[0] * flashmtx.m[0][2]) + (((f32 *) nodepos)[1] * flashmtx.m[1][2])) + (((f32 *) nodepos)[2] * flashmtx.m[2][2])) + flashmtx.m[3][2];

                    mtx4LoadAlign(&flash2mtx, (randomGetNext() * (1.0f / M_U32_MAX_VALUE_F)) * M_TAU_F, -flashpos.x, -flashpos.y, -flashpos.z);
                    mtx4ScaleRows012(IDO_POINT_ONE * flashscale, &flash2mtx);
                    mtx4LoadAlignInverse(&aimmtx, 0, gunofs.x - hand->aimPos.x, gunofs.y - hand->aimPos.y, gunofs.z - hand->aimPos.z);
                    mtx4MultInPlace(&aimmtx, &flash2mtx);
                    mtx4ScaleColumn2(flashext, &flash2mtx);
                    mtx4MultInPlace(&rotmtx, &flash2mtx);
                    mtx4SetTranslation(&flashpos, &flash2mtx);
                    mtx4Copy(&flash2mtx, &rwmtx[2]);
                }

                if (mdlhdr->Skeleton == (&skeleton_gun_kf7))
                {
                    nodeptr = (f32 *) mdlhdr->Switches[4];

                    if (nodeptr != NULL)
                    {
                        nodepos = (f32 *) ((ModelNode *) nodeptr)->Data;
                        flashpos.x = (((((f32 *) nodepos)[0] * flashmtx.m[0][0]) + (((f32 *) nodepos)[1] * flashmtx.m[1][0])) + (((f32 *) nodepos)[2] * flashmtx.m[2][0])) + flashmtx.m[3][0];
                        flashpos.y = (((((f32 *) nodepos)[0] * flashmtx.m[0][1]) + (((f32 *) nodepos)[1] * flashmtx.m[1][1])) + (((f32 *) nodepos)[2] * flashmtx.m[2][1])) + flashmtx.m[3][1];
                        flashpos.z = (((((f32 *) nodepos)[0] * flashmtx.m[0][2]) + (((f32 *) nodepos)[1] * flashmtx.m[1][2])) + (((f32 *) nodepos)[2] * flashmtx.m[2][2])) + flashmtx.m[3][2];
                        stackpad_scale = IDO_POINT_ONE * flashscale;
                        mtx4LoadAlign(&flash2mtx, (randomGetNext() * (1.0f / M_U32_MAX_VALUE_F)) * M_TAU_F, -flashpos.x, -flashpos.y, -flashpos.z);
                        mtx4ScaleRows012(stackpad_scale, &flash2mtx);
                        mtx4LoadAlignInverse(&aimmtx, 0, gunofs.x - hand->aimPos.x, gunofs.y - hand->aimPos.y, gunofs.z - hand->aimPos.z);
                        mtx4MultInPlace(&aimmtx, &flash2mtx);
                        mtx4ScaleColumn2(flashext, &flash2mtx);
                        mtx4MultInPlace(&rotmtx, &flash2mtx);
                        mtx4SetTranslation(&flashpos, &flash2mtx);
                        mtx4Copy(&flash2mtx, &rwmtx[3]);
                    }
                }
            }
        }
        else
        {
            hand->muzzlePos.x = hand->gunWorldMtx.m[3][0];
            hand->muzzlePos.y = hand->gunWorldMtx.m[3][1];
            hand->muzzlePos.z = hand->gunWorldMtx.m[3][2];
            hand->muzzleDepth = -hand->gunmtx_camspace.m[3][2];
        }

        node = mdlhdr->Switches[6];

        if (node != NULL)
        {
            swdata = (f32 *) node->Data;
            sw6mtxidx = modelFindNodeMtxIndex(node, 0);
            gunTickHingeAngle(handnum, hand->weapon_hold_time);

            if ((mdlhdr->numSwitches >= 0x1D) && (mdlhdr->Switches[28] != NULL))
            {
                hingedata = (f32 *) mdlhdr->Switches[28]->Data;
                guRotateF(tmpmtx.m, (((hand->hingeAngle + M_TAU_F) - gunGetMaxHingeAngle(handnum)) * 360.0f) / M_TAU_F, hingedata[0] - hingedata[3], hingedata[1] - hingedata[4], hingedata[2] - hingedata[5]);
                mtx4SetTranslation((coord3d *) swdata, &tmpmtx);
            }
            else
            {
                mtx4LoadYRotationAndTranslation((coord3d *) swdata, hand->hingeAngle, &tmpmtx);
            }

            mtx4MultAffine(&gunmtx, &tmpmtx, &rwmtx[sw6mtxidx]);
        }

        if (mdlhdr->numSwitches >= 0x1E)
        {
            playerSelectCuff(model, mdlhdr, 0x1D);
        }

        node = mdlhdr->Switches[7];

        if (node != NULL)
        {
            sw7data = (f32 *) node->Data;
            sw7mtxidx = modelFindNodeMtxIndex(node, 0);
            gunTickBoltRecoil(handnum);
            mtx4LoadTranslation((coord3d *) sw7data, &tmpmtx);
            tmpmtx.m[3][2] -= hand->boltRecoil;
            mtx4Mult(&gunmtx, &tmpmtx, &rwmtx[sw7mtxidx]);
        }

        shellidx = 0;

        /**
         * For the shotguns, show or hide the shells in the shell holder based on how much ammunition is in the player's reserve.
         */
        if (mdlhdr->numSwitches >= 0x13)
        {
            do
            {
                if (mdlhdr->Switches[18 + shellidx] != 0)
                {
                    nodeptr = (f32 *) modelGetNodeRwData(model, mdlhdr->Switches[18 + shellidx]);

                    if (nodeptr != NULL)
                    {
                        *((s32 *) nodeptr) = hand->numvisibleshells >= (5 - shellidx);
                    }
                }

                if (mdlhdr->Switches[23 + shellidx] != 0)
                {
                    nodeptr = (f32 *) modelGetNodeRwData(model, mdlhdr->Switches[23 + shellidx]);

                    if (nodeptr != NULL)
                    {
                        *((s32 *) nodeptr) = hand->numvisibleshells >= (5 - shellidx);
                    }
                }

                shellidx += 1;
            }
            while (shellidx != 5);
        }

        modelUpdateNodeRelations(model);

        if (hand->weapon_firing_status != 0)
        {
            switch (item)
            {
            case ITEM_WPPK:
            case ITEM_WPPKSIL:
            case ITEM_TT33:
            case ITEM_SKORPION:
            case ITEM_AK47:
            case ITEM_UZI:
            case ITEM_MP5K:
            case ITEM_MP5KSIL:
            case ITEM_SPECTRE:
            case ITEM_M16:
            case ITEM_FNP90:
            case ITEM_SNIPERRIFLE:
            case ITEM_RUGER:
            case ITEM_GOLDENGUN:
            case ITEM_SILVERWPPK:
            case ITEM_GOLDWPPK:
                gunCreateBeamForHand(handnum);
                hand->numShots = hand->numShots + 1;
                break;
            case ITEM_LASER:
            case ITEM_WATCHLASER:
#if defined(VERSION_EU)
                hand->numShots = hand->numShots + 1;
#endif
                gunCreateBeamForHand(handnum);
                break;

            default:
                break;
            }
        }
    }

    if (item == ITEM_ROCKETLAUNCH)
    {
        gunUpdateAttachedRocket(handnum);
    }

    if (hand->weapon_firing_status != 0)
    {
        gunEjectCasings(handnum, playerGetStanHeight(g_CurrentPlayer));

        if (item == ITEM_GRENADELAUNCH)
        {
            gunSpawnGLGrenade(handnum);
            return;
        }

        if (item == ITEM_GRENADE)
        {
            gunThrowGrenade(handnum);
            return;
        }

        if (item == ITEM_ROCKETLAUNCH)
        {
            gunFireTankShell(handnum);
            return;
        }

        if (item == ITEM_THROWKNIFE)
        {
            gunThrowKnife(handnum);
            return;
        }

        if ((((((((item == ITEM_REMOTEMINE) || (item == ITEM_PROXIMITYMINE)) || (item == ITEM_TIMEDMINE)) || (item == ITEM_BOMBCASE)) || (item == ITEM_BUG)) || (item == ITEM_MICROCAMERA)) || (item == ITEM_GOLDENEYEKEY)) || (item == ITEM_PLASTIQUE))
        {
            gunThrowObject(handnum);
            return;
        }

        if (item == ITEM_FLAREPISTOL)
        {
            gunSpawnGLGrenade(handnum);
            return;
        }

        if (item == ITEM_PITONGUN)
        {
            gunSpawnGLGrenade(handnum);
        }
    }
}


void gunUpdateAndFireBothHands(void)
{
    gunUpdateAndFire(GUNRIGHT);
    gunUpdateAndFire(GUNLEFT);
}


void gunRenderHands(Gfx **gdlptr)
{
    Gfx *gdl = *gdlptr;
    ModelRenderData renderdata;
    s32 handnum;
    Model *model;
 
    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};
 
    for (handnum = 0; handnum != 2; handnum++) 
    {
        struct hand *handptr = &g_CurrentPlayer->hands[handnum];
        s32 item = gunGetShownWeapon(handnum);
 
        if (handptr->handVisible == 0) 
        {
            continue;
        }
 
        if (item != ITEM_WATCHLASER) 
        {
            gdl = beamRender(gdl, &handptr->weapon_beam, 0);
        }
 
        if (item == ITEM_GOLDENGUN || item == ITEM_RUGER || item == ITEM_KNIFE || item == ITEM_THROWKNIFE || item == ITEM_SILVERWPPK || item == ITEM_GOLDWPPK) 
        {
            gSPSetLights1(gdl++, g_WeaponEnvmapLight);
            gSPLookAt(gdl++, camGetLookAt());
        }
 
        gSPPerspNormalize(gdl++, mtxCalcPerspNorm(0.0f, 300.0f));
 
        if (handptr->gunmodel.obj->numSwitches >= 0x11 && handptr->gunmodel.obj->Switches[16] != NULL)
        {
            union ModelRwData *rwdata;
            model = (Model *) &handptr->gunmodel;
            rwdata = modelGetNodeRwData(model, handptr->gunmodel.obj->Switches[17]);
 
            if (rwdata != NULL) 
            {
                rwdata->Switch.visible = 1;
            }
 
            // The taser's screen. (The rocket launcher had a sight screen here too, but its
            // model has no screen node, so it was never drawn.)
            gdl = monitorTickAndDraw(model, handptr->gunmodel.obj->Switches[16], &g_GunScreenTaser, gdl, 0, 1);
        }
 
        renderdata.gdl = gdl;
        renderdata.PropType = 4;
        renderdata.envcolour.word = g_CurrentPlayer->tileColor.a
                                  | ((u32)g_CurrentPlayer->tileColor.r << 24)
                                  | ((u32)g_CurrentPlayer->tileColor.g << 16)
                                  | ((u32)g_CurrentPlayer->tileColor.b << 8);
        renderdata.zbufferenabled = 0;
 
        mtxPushUnitWorldScale();
 
        if (item == ITEM_ROCKETLAUNCH && handptr->rocket != NULL) 
        {
            model = handptr->rocket->model;
 
            modelRender(&renderdata, model);
            mtxsF2L(model->render_pos, model->obj->numMatrices);
 
            if (handptr->firedrocket != 0) 
            {
                handptr->rocket = NULL;
            }
        }
 
        if (weaponHasFlag(item, WEAPONSTATBITFLAG_MIRROR_DUAL) != 0) 
        {
            gSPClearGeometryMode(renderdata.gdl++, G_CULL_BOTH);
 
            if (handnum == 0) 
            {
                renderdata.cullmode = 3;
            } 
            else 
            {
                renderdata.cullmode = 2;
            }
        }
 
        modelRender(&renderdata, (Model *) &handptr->gunmodel);
        gdl = renderdata.gdl;
 
        if (weaponHasFlag(item, WEAPONSTATBITFLAG_MIRROR_DUAL) != 0) 
        {
            gSPClearGeometryMode(gdl++, G_CULL_BOTH);
        }
 
        mtxsF2L(handptr->gunmodel.render_pos, handptr->gunmodel.obj->numMatrices);
        mtxPopWorldScale();
 
        gSPPerspNormalize(gdl++, viGetPerspNorm());
 
        if (item == ITEM_WATCHLASER) 
        {
            gdl = beamRender(gdl, &handptr->weapon_beam, 0);
        }
    }
 
    *gdlptr = gdl;
}


/** How far the gun dips during a reload: further for the rocket launcher. */
f32 gunGetReloadDipDepth(s32 item)
{
    return (item == ITEM_ROCKETLAUNCH) ? -80.0f : -60.0f;
}


void gunReload(enum GUNHAND hand)
{
    struct hand* hand_ptr;
    enum ITEM_IDS item_id;
    s32 ammo_in_magazine;
    s32 ammo_in_hands;
    WeaponStats* item_stats;
    s32 ammo_total;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    item_id = gunGetWeapon(hand);
    ammo_in_magazine = hand_ptr->weapon_ammo_in_magazine;
    ammo_in_hands = gunGetHeldAmmo(hand);
    item_stats = weaponGetStats(item_id);
    ammo_total = ammo_in_hands + ammo_in_magazine;

    hand_ptr->weapon_ammo_in_magazine = (ammo_total >= item_stats->MagSize)
        ? item_stats->MagSize
        : ammo_total;

    g_CurrentPlayer->ammoheldarr[item_stats->AmmoType] = (weaponHasFlag(item_id, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0)
        ? 0
        : (g_CurrentPlayer->ammoheldarr[item_stats->AmmoType] - hand_ptr->weapon_ammo_in_magazine) + ammo_in_magazine;

    if (item_id == ITEM_ROCKETLAUNCH)
    {
        gunCreateRocket(hand);
        return;
    }

    if ((item_id == ITEM_SHOTGUN) || (item_id == ITEM_AUTOSHOT))
    {
        ammo_in_hands = gunGetHeldAmmo(hand);
        if (ammo_in_hands >= 5)
        {
            hand_ptr->numvisibleshells = 5;
            return;
        }
        hand_ptr->numvisibleshells = ammo_in_hands;
    }
}

#if defined(VERSION_US)
    #define WEAPON_1P_ANIM_TIME(x) ((f32)(x))
    #define WHEN_1_CASE_GRENADELAUNCH_FLD890 6
    #define WHEN_1_CASE_GRENADE_FLD890 0xf0
    #define WHEN_D_FLD890 0x14
    #define WHEN_5_SP188_INIT 0x10
    #define WHEN_5_SP188_MULTI 0xc
    #define WHEN_5_FLD8B0_SP 0x11
    #define WHEN_5_FLD8B0_MULTI 0xd
    #define WHEN_8_SP178_INIT 0x17
    #define WHEN_8_SP178_MULTI 0xc
    #define WHEN_A_FLD890 0x10
    #define WHEN_A_FLD8B0 0x11
    #define WHEN_C_FLD890 0x17
    #define WHEN_E_FLD890 0x10
    #define WHEN_10_FLD890 0x17
    #define WHEN_11_FLD890_1 0x10
    #define WHEN_11_FLD890_2 0x18
    #define WHEN_1E_FLD890 0x1e
#endif
#if defined(VERSION_EU)
    #define WEAPON_1P_ANIM_TIME(x) ((f32)(x)) * 60.0f / 50.0f
    #define WHEN_1_CASE_GRENADELAUNCH_FLD890 5
    #define WHEN_1_CASE_GRENADE_FLD890 0xc8
    #define WHEN_D_FLD890 0x10
    #define WHEN_5_SP188_INIT 0xd
    #define WHEN_5_SP188_MULTI 0xa
    #define WHEN_5_FLD8B0_SP 0xe
    #define WHEN_5_FLD8B0_MULTI 0xa
    #define WHEN_8_SP178_INIT 0x13
    #define WHEN_8_SP178_MULTI 0xa
    #define WHEN_A_FLD890 0xd
    #define WHEN_A_FLD8B0 0xe
    #define WHEN_C_FLD890 0x13
    #define WHEN_E_FLD890 0xd
    #define WHEN_10_FLD890 0x13
    #define WHEN_11_FLD890_1 0xd
    #define WHEN_11_FLD890_2 0x14
    #define WHEN_1E_FLD890 0x19
#endif


void gunTickHandState(enum GUNHAND hand, s32 triggerOn)
{
#if defined(VERSION_US)
    s32 ammoType;
    struct hand *other;
    s32 playSound;
    struct WeaponStats *weapon_stats;
    s32 recoilTicks;
    s32 settleTicks;
    f32 recoilUp;
    f32 recoilBack;
    f32 recoilFrac;
    f32 returnFrac;
    s32 lowerTime;
    f32 lowerAngle;
    s32 raiseTime;
    f32 raiseAngle;
    Mtxf dipMtx;
    f32 reloadLowerAngle;
    Mtxf swapDipMtx;
    f32 tempf;
    struct hand *handptr;
    Mtxf raiseDipMtx;
    f32 reloadRaiseAngle;
    f32 watchLowerAngle;
    enum ITEM_IDS otherWeapon;
    f32 watchRaiseAngle;
    f32 slashTime;
    enum ITEM_IDS weapon;
    struct PropRecord *prop;
    Weapon1PTransformKeyframe *punchKeyframes;
    f32 recoilAngle;
    Weapon1PTransformKeyframe *slashKeyframes;
    f32 punchTime;
    struct hand *otherhand;
    f32 tiltAngle = 0.0f;
    f32 lowerTicks = 16.0f;
    f32 raiseTicks = 23.0f;
#endif
#if defined(VERSION_EU)
    s32 ammoType;
    struct hand *other;
    s32 playSound;
    struct WeaponStats *weapon_stats;
    s32 recoilTicks;
    s32 settleTicks;
    f32 recoilUp;
    s32 stat_2;
    s32 stat_3;
    s32 stat_4;
    f32 recoilBack;
    f32 recoilFrac;
    f32 returnFrac;
    s32 lowerTime;
    f32 lowerAngle;
    s32 raiseTime;
    f32 raiseAngle;
    Mtxf dipMtx;
    f32 reloadLowerAngle;
    Mtxf swapDipMtx;
    f32 tempf;
    Mtxf raiseDipMtx;
    f32 reloadRaiseAngle;
    f32 watchLowerAngle;
    struct hand *handptr;
    f32 watchRaiseAngle;
    f32 slashTime;
    enum ITEM_IDS otherWeapon;
    enum ITEM_IDS weapon;
    Weapon1PTransformKeyframe *punchKeyframes;
    struct PropRecord *prop;
    f32 recoilAngle;
    Weapon1PTransformKeyframe *slashKeyframes;
    f32 punchTime;
    struct hand *otherhand;
    f32 tiltAngle = 0.0f;
    f32 lowerTicks = 13.0f;
    f32 raiseTicks = 19.0f;
#endif

    handptr = &g_CurrentPlayer->hands[hand];
    weapon = gunGetShownWeapon(hand);
    ammoType = weaponGetAmmoType(weapon);

    handptr->weapon_hold_time = triggerOn;

    if (triggerOn == 0)
    {
        handptr->triggerReleased = 1;
    }

    handptr->weapon_firing_status = 0;
    handptr->firing = 0;

    if (g_ClockTimer > 0)
    {
        handptr->stateTime += g_ClockTimer;
        handptr->stateFrames += 1;
    }

    handptr->useAnimMtx = 0;

    if (handptr->weapon_action_state == GUN_ANIM_STATE_IDLE)
    {
#if defined(VERSION_EU)
        if ((weapon == ITEM_LASER) && (handptr->triggerReleased != 0))
        {
            handptr->numShots = 0;
        }
#endif
        if (
            (handptr->weapon_hold_time != 0)
            && (weapon != ITEM_UNARMED)
            && (((weaponHasFlag(weapon, WEAPONSTATBITFLAG_CLICKY) != 0)) || (handptr->weapon_ammo_in_magazine > 0))
#if defined(VERSION_EU)
            && ((weapon != ITEM_LASER) || (handptr->numShots < 0xC8))
#endif
        )
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_TRIGGER_PRESS;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            handptr->triggerReleased = 0;
        }
        else
        {
            if (handptr->weapon_current_animation != 0)
            {
                handptr->weapon_action_state = handptr->weapon_current_animation;
                handptr->stateTime = 0;
                handptr->stateFrames = 0;
            }
        }

        handptr->weapon_current_animation = 0;

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_IDLE)
            && (handptr->weapon_ammo_in_magazine == 0)
            && (ammoType != 0))
        {
            if ((lvGetControlsLocked() == 0) && (g_CurrentPlayer->mpmenuon == 0))
            {
                if (gunGetHeldAmmo(hand) > 0)
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_START;
                    handptr->stateTime = 0;
                    handptr->stateFrames = 0;
                }
                else
                {
                    if (g_CurrentPlayer->trigger_released != 0)
                    {
                        otherWeapon = gunGetShownWeapon(1 - hand);

                        other = (g_CurrentPlayer->hands - hand) + 1;

                        if ((other->weapon_action_state == GUN_ANIM_STATE_IDLE)
                            && (other->weapon_current_animation == 0)
                            && (
                                (otherWeapon == ITEM_UNARMED)
                                || ((other->weapon_ammo_in_magazine == 0)
                                    && ((weaponGetAmmoType(otherWeapon) != 0))
                                    && ((gunGetHeldAmmo(1 - hand) <= 0)))))
                        {
                            gunSwitchWhenEmpty();

                            handptr->stateFrames = 0;
                            handptr->stateTime = 0;
                            handptr->weapon_action_state = handptr->weapon_current_animation;
                            handptr->weapon_current_animation = 0;

                            other->stateFrames = 0;
                            other->stateTime = 0;
                            other->weapon_action_state = other->weapon_current_animation;
                            other->weapon_current_animation = 0;
                        }
                    }
                }
            }
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_TRIGGER_PRESS)
    {
        switch (weapon)
        {
        case ITEM_RUGER:
        case ITEM_GRENADELAUNCH:
            if (handptr->stateTime >= WHEN_1_CASE_GRENADELAUNCH_FLD890)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
                handptr->stateTime = 0;
                handptr->stateFrames = 0;
            }
            break;
        case ITEM_CAMERA:
            if (handptr->stateFrames == 0)
            {
                currentPlayerSetFadeColour(0, 0, 0, 1.0f);
            }
            else if (handptr->stateTime > 0)
            {
                currentPlayerAdjustFade(8.0f, 0, 0, 0, 0.0f);
                handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
                handptr->stateTime = 0;
                handptr->stateFrames = 0;
            }
            break;
        case ITEM_WPPK:
        case ITEM_WPPKSIL:
        case ITEM_TT33:
        case ITEM_SKORPION:
        case ITEM_AK47:
        case ITEM_UZI:
        case ITEM_MP5K:
        case ITEM_MP5KSIL:
        case ITEM_SPECTRE:
        case ITEM_M16:
        case ITEM_FNP90:
        case ITEM_SHOTGUN:
        case ITEM_AUTOSHOT:
        case ITEM_SNIPERRIFLE:
        case ITEM_GOLDENGUN:
        case ITEM_SILVERWPPK:
        case ITEM_GOLDWPPK:
        case ITEM_LASER:
        case ITEM_WATCHLASER:
        case ITEM_ROCKETLAUNCH:
        case ITEM_TRIGGER:
        case ITEM_TANKSHELLS:
        case ITEM_FLAREPISTOL:
        case ITEM_PITONGUN:
        case ITEM_WATCHMAGNETATTRACT:
            handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        case ITEM_TIMEDMINE:
        case ITEM_PROXIMITYMINE:
        case ITEM_REMOTEMINE:
        case ITEM_BOMBCASE:
        case ITEM_PLASTIQUE:
        case ITEM_BUG:
        case ITEM_MICROCAMERA:
        case ITEM_GOLDENEYEKEY:
            handptr->weapon_action_state = GUN_ANIM_STATE_MINE_PLACE;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        case ITEM_KNIFE:
            if (!(randomGetNext() & 1))
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_BEGIN;
            }
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        case ITEM_GRENADE:
            if ((handptr->triggerReleased != 0) || (handptr->stateTime >= WHEN_1_CASE_GRENADE_FLD890))
            {
                g_CurrentPlayer->last_z_trigger_timer = handptr->stateTime;
                handptr->weapon_action_state = GUN_ANIM_STATE_GRENADE_THROW;
                handptr->stateFrames = 0;
                handptr->stateTime = 0;
            }
            break;
        case ITEM_FIST:
            if (!(randomGetNext() & 1))
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH1_STRIKE;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH2_STRIKE;
            }
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        case ITEM_THROWKNIFE:
            handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_DRAW;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        case ITEM_TASER:
            tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
            if (gunSample1PTransform(taserFireKeyFrames, tempf, &handptr->animMtx, hand) != 0)
            {
                handptr->useAnimMtx = 1;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_FIRE;
                handptr->stateTime = 0;
                handptr->stateFrames = 0;
            }
            break;
        case ITEM_BUNGEE:
        case ITEM_DOORDECODER:
        case ITEM_BOMBDEFUSER:
        case ITEM_LOCKEXPLODER:
        case ITEM_DOOREXPLODER:
        case ITEM_WEAPONCASE:
        case ITEM_SAFECRACKERCASE:
        case ITEM_KEYANALYSERCASE:
        case ITEM_BUGDETECTOR:
        case ITEM_EXPLOSIVEFLOPPY:
        case ITEM_POLARIZEDGLASSES:
        case ITEM_DARKGLASSES:
        case ITEM_CREDITCARD:
        case ITEM_GASKEYRING:
        case ITEM_DATATHIEF:
        case ITEM_WATCHIDENTIFIER:
        case ITEM_WATCHCOMMUNICATOR:
        case ITEM_WATCHGEIGERCOUNTER:
        case ITEM_WATCHMAGNETREPEL:
        case ITEM_DATTAPE:
        case ITEM_KEYCARD:
        case ITEM_KEYYALE:
        case ITEM_KEYBOLT:
            handptr->weapon_action_state = GUN_ANIM_STATE_USE_ITEM;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        case ITEM_SUIT_LF_HAND:
        case ITEM_JOYPAD:
        case ITEM_ROCKETROUND:
        case ITEM_GRENADEROUND:
        case ITEM_TOKEN:
        default:
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
            break;
        }

        handptr->volley = 0;
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_FIRE)
    {
        if ((weaponGetAmmoType(weapon) == 0) || (handptr->weapon_ammo_in_magazine > 0))
        {
            switch (weapon)
            {
            case ITEM_CAMERA:
            case ITEM_WATCHMAGNETATTRACT:
                if (handptr->stateFrames == 0)
                {
                    handptr->weapon_firing_status = (lvGetControlsLocked() == 0) && (g_CurrentPlayer->mpmenuon == 0);
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->stateTime = 0;
                    handptr->stateFrames = 0;
                }
                break;
            case ITEM_WPPK:
            case ITEM_WPPKSIL:
            case ITEM_TT33:
            case ITEM_SHOTGUN:
            case ITEM_AUTOSHOT:
            case ITEM_SNIPERRIFLE:
            case ITEM_RUGER:
            case ITEM_GOLDENGUN:
            case ITEM_SILVERWPPK:
            case ITEM_GOLDWPPK:
            case ITEM_LASER:
            case ITEM_WATCHLASER:
            case ITEM_GRENADELAUNCH:
            case ITEM_ROCKETLAUNCH:
            case ITEM_TRIGGER:
            case ITEM_TANKSHELLS:
            case ITEM_FLAREPISTOL:
            case ITEM_PITONGUN:
                if (handptr->stateFrames == 0)
                {
                    if ((!isMpGame()) || ((mpIsPaused() == 0) && (g_CurrentPlayer->mpmenuon == 0)))
                    {
                        handptr->firing = 1;
                    }

                    handptr->weapon_firing_status = (lvGetControlsLocked() == 0) && (g_CurrentPlayer->mpmenuon == 0);

                    gunStartBoltRecoil(hand);
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->stateTime = 0;
                    handptr->stateFrames = 0;
                }
                break;
            case ITEM_SKORPION:
            case ITEM_AK47:
            case ITEM_UZI:
            case ITEM_MP5K:
            case ITEM_MP5KSIL:
            case ITEM_SPECTRE:
            case ITEM_M16:
            case ITEM_FNP90:
                if ((handptr->stateFrames == 0)
                    || (handptr->weapon_hold_time != 0)
                    || ((weaponHasFlag(weapon, WEAPONSTATBITFLAG_BURST_FIRE) != 0)
                        && (currentPlayerGetIsAiming() == 0)
                        && (((s32) handptr->volley % 3) != 0)))
                {
                    if (((s32) handptr->stateFrames % weaponGetFireRate(weapon)) == 0)
                    {
                        if ((!isMpGame()) || ((mpIsPaused() == 0) && (g_CurrentPlayer->mpmenuon == 0)))
                        {
                            handptr->firing = 1;
                        }

                        handptr->weapon_firing_status = (lvGetControlsLocked() == 0)
                            && (g_CurrentPlayer->mpmenuon == 0);
                    }
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->stateTime = 0;
                    handptr->stateFrames = 0;
                }
                break;
            case ITEM_KNIFE:
                if ((handptr->stateFrames == 0) || (handptr->weapon_hold_time != 0))
                {
                    handptr->weapon_firing_status = 0;
                    handptr->firing = handptr->weapon_firing_status;
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->stateTime = 0;
                    handptr->stateFrames = 0;
                }
                break;
            case ITEM_TASER:
                if ((handptr->stateFrames == 0) || (handptr->weapon_hold_time != 0))
                {
                    gunSample1PTransform(taserRaiseKeyframes, 0.0f, &handptr->animMtx, hand);

                    handptr->weapon_firing_status = 0;
                    handptr->useAnimMtx = 1;
                    handptr->firing = handptr->weapon_firing_status;

                    if (handptr->stateFrames == 0)
                    {
                        handptr->weapon_firing_status = (lvGetControlsLocked() == 0)
                            && (g_CurrentPlayer->mpmenuon == 0);
                    }
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
                    handptr->stateTime = 0;
                    handptr->stateFrames = 0;
                }
                break;

            default:
                break;
            }

            if (handptr->weapon_firing_status != 0)
            {
                if (weapon != ITEM_CAMERA)
                {
                    joyRumblePakStart(playerGetCurrentNum(), 0.1f);

                    if (optionsGetControlStyle() >= 4)
                    {
                        joyRumblePakStart(playerGetCurrentNum() + getPlayerCount(), 0.1f);
                    }
                }

                handptr->weapon_ammo_in_magazine -= 1;
                handptr->volley += 1;
            }

            if (handptr->weapon_action_state == GUN_ANIM_STATE_FIRE)
            {
                playSound = 0;

                if (weaponGetSoundRate(weapon) > 0)
                {
                    if ((g_CurrentPlayer->hands[1 - hand].lastSoundTime != g_GlobalTimer)
                        && (handptr->nextSoundTime < g_GlobalTimer))
                    {
                        handptr->nextSoundTime = weaponGetSoundRate(weapon) + g_GlobalTimer;
                        playSound = 1;
                    }
                }
                else if (handptr->weapon_firing_status != 0)
                {
                    playSound = 1;
                }

                if ((!isMpGame()) || ((mpIsPaused() == 0) && (g_CurrentPlayer->mpmenuon == 0)))
                {
                    if (playSound != 0)
                    {
                        if ((handptr->audioHandle != NULL) && (sndGetPlayingState(handptr->audioHandle) != 0))
                        {
                            sndDeactivate(handptr->audioHandle);
                        }

                        if ((handptr->audioHandle2 != 0)
                            && (sndGetPlayingState(handptr->audioHandle2) != 0))
                        {
                            sndDeactivate(handptr->audioHandle2);
                        }

                        if (weaponGetSound(weapon) != 0)
                        {
                            if (handptr->audioHandle == NULL)
                            {
                                sndPlaySfx(g_musicSfxBufferPtr, weaponGetSound(weapon), &handptr->audioHandle);
                            }
                            else if (handptr->audioHandle2 == 0)
                            {
                                sndPlaySfx(g_musicSfxBufferPtr, weaponGetSound(weapon), &handptr->audioHandle2);
                            }

                            handptr->lastSoundTime = g_GlobalTimer;
                        }
                    }

                    if (weapon == ITEM_WATCHLASER)
                    {
                        sndPlaySfx(g_musicSfxBufferPtr, watchlaser_fire_sounds[randomGetNext() & 1], NULL);
                    }
                }
            }
        }
        else if (handptr->stateFrames > 0)
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL1;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_DRY_FIRE;
            handptr->stateTime = 0;
            handptr->stateFrames = 0;

            if ((!isMpGame())
                || ((mpIsPaused() == 0) && (g_CurrentPlayer->mpmenuon == 0))
               )
            {
                sndPlaySfx(g_musicSfxBufferPtr, EMPTY_GUN_FIRE_SFX, NULL);
            }
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RECOIL1)
    {
        if (weapon == ITEM_TASER)
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
            if (gunSample1PTransform(taserRaiseKeyframes, tempf, &handptr->animMtx, hand) != 0)
            {
                handptr->useAnimMtx = 1;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
                handptr->stateTime = 0.0f;
                handptr->stateFrames = 0;
            }
        }
        else
        {
            weapon_stats = weaponGetStats(weapon);

#if defined(VERSION_US)
            recoilTicks = weapon_stats->b44[0];
            settleTicks = weapon_stats->b44[1];
#endif
#if defined(VERSION_EU)
            recoilTicks = ((s32)weapon_stats->b44[0] * 50) / 60;
            settleTicks = ((s32)weapon_stats->b44[1] * 50) / 60;
            stat_2 = ((s32)weapon_stats->b44[2] * 50) / 60;
            stat_3 = ((s32)weapon_stats->b44[3] * 50) / 60;
            stat_4 = weapon_stats->SingleFiringRate * 50 / 60;
#endif

            if ((
                    (handptr->triggerReleased != 0)
                    && (handptr->stateTime >= (recoilTicks + settleTicks))
                )
                ||
                (
                    ((weapon_stats->SingleFiringRate >= 0))
                    && (handptr->triggerReleased == 0)
#if defined(VERSION_US)
                    && (handptr->stateTime >= (recoilTicks + settleTicks + weapon_stats->SingleFiringRate))
#endif
#if  defined(VERSION_EU)
                    && (handptr->stateTime >= (recoilTicks + settleTicks + stat_4))
#endif
                )
               )
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
                handptr->stateTime = 0.0f;
                handptr->stateFrames = 0;
            }
            else if (
                (handptr->triggerReleased != 0)
                && (handptr->weapon_hold_time != 0)

#if defined(VERSION_US)
                && (handptr->stateTime >= weapon_stats->b44[2])
#endif
#if  defined(VERSION_EU)
                && (handptr->stateTime >= stat_2)
#endif

                && (weapon_stats->b44[3] >= 0)

#if defined(VERSION_US)
                // HACK: registers are swapped
                // addu a1, v1, a0
                && (handptr->stateTime + weapon_stats->b44[3] < recoilTicks + settleTicks)
                && (handptr->stateTime + weapon_stats->b44[3] >= (s32)weapon_stats->b44[2])
#endif
#if  defined(VERSION_EU)
                && (handptr->stateTime + stat_3 < recoilTicks + settleTicks)
                && (handptr->stateTime + stat_3 >= (s32)stat_2)
#endif
            )
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_RECOIL2;
                handptr->stateTime = 0;
                handptr->stateFrames = 0;
#if defined(VERSION_US)
                handptr->recoilReturnTime = weapon_stats->b44[3];
#endif
#if  defined(VERSION_EU)
                handptr->recoilReturnTime = stat_3;
#endif
            }
            else if (handptr->stateTime < recoilTicks + settleTicks)
            {
                recoilBack = weapon_stats->RecoilBack;
                recoilUp = weapon_stats->RecoilUp;

                if (handptr->stateTime == 0)
                {
                    handptr->recoilFromAngle = handptr->recoilAngle;
                    handptr->recoilFromPos.x = handptr->recoilPos.x;
                    handptr->recoilFromPos.y = handptr->recoilPos.y;
                    handptr->recoilFromPos.z = handptr->recoilPos.z;
                }

                if (handptr->stateTime < recoilTicks)
                {
                    handptr->recoilToAngle = M_TAU_F - ((recoilUp * M_TAU_F) / 360.0f);

                    handptr->recoilToPos.x = ((gunSetHorizontalOffset(hand) - handptr->aimPos.x) * recoilBack) / 1000.0f;
                    handptr->recoilToPos.y = 0;
                    handptr->recoilToPos.z = ((weapon_stats->PosZ - handptr->aimPos.z) * recoilBack) / 1000.0f;

                    recoilFrac = sinf(((f32) handptr->stateTime * M_PI_2F) / (f32) recoilTicks);
                }
                else
                {
                    handptr->recoilToAngle = M_TAU_F - ((recoilUp * M_TAU_F) / 360.0f);

                    handptr->recoilToPos.x = ((gunSetHorizontalOffset(hand) - handptr->aimPos.x) * recoilBack) / 1000.0f;
                    handptr->recoilToPos.y = 0;
                    handptr->recoilToPos.z = ((weapon_stats->PosZ - handptr->aimPos.z) * recoilBack) / 1000.0f;

                    recoilFrac = (cosf(((f32) (handptr->stateTime - recoilTicks) * M_PI_F) / (f32) settleTicks) * 0.5f) + 0.5f;
                }

                recoilAngle = modelLerpAngle(handptr->recoilFromAngle, handptr->recoilToAngle, recoilFrac);

                handptr->recoilAngle = recoilAngle;
                handptr->useAnimMtx = 1;
                handptr->recoilPos.x = ((handptr->recoilToPos.x - handptr->recoilFromPos.x) * recoilFrac) + handptr->recoilFromPos.x;
                handptr->recoilPos.y = ((handptr->recoilToPos.y - handptr->recoilFromPos.y) * recoilFrac) + handptr->recoilFromPos.y;
                handptr->recoilPos.z = ((handptr->recoilToPos.z - handptr->recoilFromPos.z) * recoilFrac) + handptr->recoilFromPos.z;

                mtx4LoadXRotation(recoilAngle, &handptr->animMtx);
                mtx4SetTranslation((coord3d *)&handptr->recoilPos.x, &handptr->animMtx);
            }
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RECOIL2)
    {
        if (handptr->stateTime == 0)
        {
            handptr->recoilFromAngle = handptr->recoilAngle;
            handptr->recoilFromPos.x = handptr->recoilPos.x;
            handptr->recoilFromPos.y = handptr->recoilPos.y;
            handptr->recoilFromPos.z = handptr->recoilPos.z;
            handptr->recoilToAngle = 0.0f;
            handptr->recoilToPos.x = 0.0f;
            handptr->recoilToPos.y = 0.0f;
            handptr->recoilToPos.z = 0.0f;
        }

        if (handptr->stateTime < handptr->recoilReturnTime)
        {
            returnFrac = (cosf(((f32) (handptr->recoilReturnTime - handptr->stateTime) * M_PI_2F) / (f32) handptr->recoilReturnTime) * 0.5f) + 0.5f;

            recoilAngle = modelLerpAngle(handptr->recoilFromAngle, handptr->recoilToAngle, returnFrac);

            handptr->recoilAngle = recoilAngle;
            handptr->useAnimMtx = 1;
            handptr->recoilPos.x = ((handptr->recoilToPos.x - handptr->recoilFromPos.x) * returnFrac) + handptr->recoilFromPos.x;
            handptr->recoilPos.y = ((handptr->recoilToPos.y - handptr->recoilFromPos.y) * returnFrac) + handptr->recoilFromPos.y;
            handptr->recoilPos.z = ((handptr->recoilToPos.z - handptr->recoilFromPos.z) * returnFrac) + handptr->recoilFromPos.z;

            mtx4LoadXRotation(recoilAngle, &handptr->animMtx);
            mtx4SetTranslation((coord3d *)&handptr->recoilPos.x, &handptr->animMtx);
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_DRY_FIRE)
    {
        if (handptr->stateFrames == 0)
        {
            gunStartBoltRecoil(hand);
        }

        if ((handptr->triggerReleased != 0) || ((handptr->triggerReleased == 0) && (handptr->stateTime >= WHEN_D_FLD890)))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    lowerTime = WHEN_5_SP188_INIT;
    if (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_LOWER)
    {
        if (getPlayerCount() >= 2)
        {
            lowerTime = WHEN_5_SP188_MULTI;
        }

        if (handptr->stateFrames == 0)
        {
            if (getPlayerCount() == 1)
            {
                handptr->switchTime = WHEN_5_FLD8B0_SP;
            }
            else
            {
                handptr->switchTime = WHEN_5_FLD8B0_MULTI;
            }
        }

        if (handptr->stateTime >= lowerTime)
        {
            g_CurrentPlayer->ammoheldarr[weaponGetAmmoType(weapon)] += handptr->weapon_ammo_in_magazine;
            handptr->weapon_ammo_in_magazine = 0;

            if (getPlayerCount() >= 2)
            {
                playerBodyRemoveGun(hand);
            }

            gunFreeRocket(hand);

            handptr->weapon_action_state = GUN_ANIM_STATE_SWITCH_SWAP;

            if (invItemAvailable(ITEM_SNIPERRIFLE) != 0)
            {
                g_CurrentPlayer->cur_item_weapon_getname = ITEM_SNIPERRIFLE;
            }
            else
            {
                g_CurrentPlayer->cur_item_weapon_getname = ITEM_FIST;
            }
        }
        else
        {
            lowerAngle = ((f32) handptr->stateTime * M_LN2F) / (f32) lowerTime;
            handptr->useAnimMtx = 1;

            mtx4LoadXRotation(lowerAngle, &handptr->animMtx);

            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = (1.0f - cosf(lowerAngle)) * -60.0f;
            handptr->animMtx.m[3][2] = sinf(lowerAngle) * 15.0f;
        }
    }

    if ((handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP) || (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_HOLD))
    {
        if ((handptr->weapon_animation_trigger == 0) || (handptr->stateTime >= handptr->switchTime))
        {
            if (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP)
            {
                otherhand = (g_CurrentPlayer->hands - hand) + 1;

                if ((otherhand->weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP) && (otherhand->weapon_action_state != GUN_ANIM_STATE_SWITCH_LOWER))
                {
                    if (
                        (otherhand->weapon_current_animation != 5)
                        && (otherhand->weapon_action_state != GUN_ANIM_STATE_WATCH_LOWER)
                        && (otherhand->weapon_action_state != GUN_ANIM_STATE_WATCH_SWAP)
                        && (otherhand->weapon_action_state != GUN_ANIM_STATE_WATCH_RAISE)
                        && (otherhand->weapon_current_animation != 0xE))
                    {
                        if (hand == GUNRIGHT)
                        {
                            if (invItemAvailableForHand(handptr->weapon_next_weapon, gunGetWeapon(GUNLEFT)) == 0)
                            {
                                gunEquip(GUNLEFT, 0);
                            }
                        }
                        else if (invItemAvailableForHand(gunGetWeapon(GUNRIGHT), handptr->weapon_next_weapon) == 0)
                        {
                            handptr->weapon_next_weapon = ITEM_UNARMED;
                        }
                    }
                }
                gunUnequip(hand, handptr->weapon_next_weapon);
                weapon = gunGetShownWeapon(hand);
                handptr->weapon_action_state = GUN_ANIM_STATE_SWITCH_HOLD;
            }
            else if (gunHandModelIsEmpty(hand) != 0)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_SWITCH_RAISE;
                handptr->stateTime = 0.0f;
                handptr->stateFrames = 0;
            }
        }

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_SWAP) || (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_HOLD))
        {
            handptr->useAnimMtx = 1;
            mtx4LoadXRotation(M_LN2F, &handptr->animMtx);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = (1.0f - cosf(M_LN2F)) * -60.0f;
            handptr->animMtx.m[3][2] = sinf(M_LN2F) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_SWITCH_RAISE)
    {
        raiseTime = WHEN_8_SP178_INIT;

        if (getPlayerCount() >= 2)
        {
            raiseTime = WHEN_8_SP178_MULTI;
        }

        if (handptr->stateFrames == 0)
        {
            if (getPlayerCount() >= 2)
            {
                playerBodyGiveGun(hand);
            }

            gunReload(hand);

            g_CurrentPlayer->trigger_released = 0;

            if ((g_ClockTimer > 0)
                && (g_CurrentPlayer->cameramode != CAMERAMODE_INTRO)
                && (gunHandModelIsEmpty(hand) != 0)
                && (g_PlayerInvincible == FALSE)
                && (g_CurrentPlayer->bonddead == 0)
               )
            {
                switch (weapon)
                {
                    case ITEM_LASER:
                        sndPlaySfx(g_musicSfxBufferPtr, PICKUP_LASER_SFX, NULL);
                        break;

                    case ITEM_KNIFE:
                    case ITEM_THROWKNIFE:
                        sndPlaySfx(g_musicSfxBufferPtr, PICKUP_KNIFE_SFX, NULL);
                        break;

                    case ITEM_TIMEDMINE:
                    case ITEM_PROXIMITYMINE:
                    case ITEM_REMOTEMINE:
                        sndPlaySfx(g_musicSfxBufferPtr, PICKUP_MINE_SFX, NULL);
                        break;

                    case ITEM_UNARMED:
                    case ITEM_FIST:
                    case ITEM_WATCHLASER:
                    case ITEM_GRENADE:
                    case ITEM_TRIGGER:
                    case ITEM_TASER:
                    case ITEM_TANKSHELLS:
                    case ITEM_BOMBCASE:
                    case ITEM_PLASTIQUE:
                    case ITEM_CAMERA:
                    case ITEM_BUG:
                    case ITEM_MICROCAMERA:
                    case ITEM_WATCHMAGNETATTRACT:
                    case ITEM_GOLDENEYEKEY:
                    case ITEM_TOKEN:
                        break;

                    default:
                        sndPlaySfx(g_musicSfxBufferPtr, PICKUP_GUN_SFX, NULL);
                        break;
                }
            }
        }

        if ((handptr->stateTime >= raiseTime)
            || (get_ptr_weapon_model_header_line(weapon) == NULL)
            || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)
            || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            raiseAngle = ((f32) (raiseTime - handptr->stateTime) * M_LN2F) / (f32) raiseTime;
            handptr->useAnimMtx = 1;
            mtx4LoadXRotation(raiseAngle, &handptr->animMtx);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = (1.0f - cosf(raiseAngle)) * -60.0f;
            handptr->animMtx.m[3][2] = sinf(raiseAngle) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_START)
    {
        if (((handptr->weapon_ammo_in_magazine < weaponGetStats(weapon)->MagSize)
             || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0))
            && ((gunGetHeldAmmo(hand) > 0)))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_LOWER;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_LOWER)
    {
        if ((handptr->stateTime >= WHEN_A_FLD890) || (handptr->handVisible == 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_SWAP;
            handptr->switchTime = WHEN_A_FLD8B0;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            reloadLowerAngle = ((f32) handptr->stateTime * M_LN2F) / lowerTicks;
            handptr->useAnimMtx = 1;

            if (hand == GUNRIGHT)
            {
                mtx4LoadZRotation((tiltAngle / lowerTicks), &handptr->animMtx);
            }
            else
            {
                mtx4LoadZRotation(-(tiltAngle / lowerTicks), &handptr->animMtx);
            }

            mtx4LoadXRotation(reloadLowerAngle, &dipMtx);
            mtx4MultInPlace(&dipMtx, &handptr->animMtx);
            sinf((tiltAngle / lowerTicks));
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = gunGetReloadDipDepth(weapon) * (1.0f - cosf(reloadLowerAngle));
            handptr->animMtx.m[3][2] = sinf(reloadLowerAngle) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_SWAP)
    {
        if ((handptr->stateFrames == 0)
            && (g_ClockTimer > 0)
            && (g_CurrentPlayer->cameramode != CAMERAMODE_INTRO)
            && (gunHandModelIsEmpty(hand) != 0)
            && (g_PlayerInvincible == FALSE)
            && (g_CurrentPlayer->bonddead == 0)
           )
        {
            switch (weapon)
            {
            case ITEM_UNARMED:
            case ITEM_FIST:
            case ITEM_KNIFE:
            case ITEM_THROWKNIFE:
            case ITEM_LASER:
            case ITEM_WATCHLASER:
            case ITEM_GRENADE:
            case ITEM_TIMEDMINE:
            case ITEM_PROXIMITYMINE:
            case ITEM_REMOTEMINE:
            case ITEM_TRIGGER:
            case ITEM_TASER:
            case ITEM_TANKSHELLS:
            case ITEM_BOMBCASE:
            case ITEM_PLASTIQUE:
            case ITEM_CAMERA:
            case ITEM_BUG:
            case ITEM_MICROCAMERA:
            case ITEM_WATCHMAGNETATTRACT:
            case ITEM_GOLDENEYEKEY:
            case ITEM_TOKEN:
                break;
            default:
            case ITEM_WPPK:
            case ITEM_WPPKSIL:
            case ITEM_TT33:
            case ITEM_SKORPION:
            case ITEM_AK47:
            case ITEM_UZI:
            case ITEM_MP5K:
            case ITEM_MP5KSIL:
            case ITEM_SPECTRE:
            case ITEM_M16:
            case ITEM_FNP90:
            case ITEM_SHOTGUN:
            case ITEM_AUTOSHOT:
            case ITEM_SNIPERRIFLE:
            case ITEM_RUGER:
            case ITEM_GOLDENGUN:
            case ITEM_SILVERWPPK:
            case ITEM_GOLDWPPK:
            case ITEM_GRENADELAUNCH:
            case ITEM_ROCKETLAUNCH:
            case ITEM_FLAREPISTOL:
            case ITEM_PITONGUN:
            case ITEM_BUNGEE:
            case ITEM_DOORDECODER:
            case ITEM_BOMBDEFUSER:
            case ITEM_LOCKEXPLODER:
            case ITEM_DOOREXPLODER:
            case ITEM_BRIEFCASE:
            case ITEM_WEAPONCASE:
            case ITEM_SAFECRACKERCASE:
            case ITEM_KEYANALYSERCASE:
            case ITEM_BUGDETECTOR:
            case ITEM_EXPLOSIVEFLOPPY:
            case ITEM_POLARIZEDGLASSES:
            case ITEM_DARKGLASSES:
            case ITEM_CREDITCARD:
            case ITEM_GASKEYRING:
            case ITEM_DATATHIEF:
            case ITEM_WATCHIDENTIFIER:
            case ITEM_WATCHCOMMUNICATOR:
            case ITEM_WATCHGEIGERCOUNTER:
            case ITEM_WATCHMAGNETREPEL:
                sndPlaySfx(g_musicSfxBufferPtr, GUN_RIFLECOCK_SFX, NULL);
                break;
            }
        }

        if ((handptr->stateTime >= handptr->switchTime) && !(((handptr->stateFrames < 2))))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_RELOAD_RAISE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            handptr->useAnimMtx = 1;

            if (hand == GUNRIGHT)
            {
                mtx4LoadZRotation(tiltAngle, &handptr->animMtx);
            }
            else
            {
                mtx4LoadZRotation(-tiltAngle, &handptr->animMtx);
            }

            mtx4LoadXRotation(M_LN2F, &swapDipMtx);
            mtx4MultInPlace(&swapDipMtx, &handptr->animMtx);
            sinf(tiltAngle);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = gunGetReloadDipDepth(weapon) * (1.0f - cosf(M_LN2F));
            handptr->animMtx.m[3][2] = sinf(M_LN2F) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_RELOAD_RAISE)
    {
        if (handptr->stateFrames == 0)
        {
            gunReload(hand);
            g_CurrentPlayer->trigger_released = 0;
        }

        if ((handptr->stateTime >= WHEN_C_FLD890)
            || (get_ptr_weapon_model_header_line(weapon) == NULL)
            || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)
            || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            reloadRaiseAngle = ((f32) (WHEN_C_FLD890 - handptr->stateTime) * M_LN2F) / raiseTicks;
            handptr->useAnimMtx = 1;

            if (hand == GUNRIGHT)
            {
                mtx4LoadZRotation((tiltAngle / raiseTicks), &handptr->animMtx);
            }
            else
            {
                mtx4LoadZRotation(-(tiltAngle / raiseTicks), &handptr->animMtx);
            }

            mtx4LoadXRotation(reloadRaiseAngle, &raiseDipMtx);
            mtx4MultInPlace(&raiseDipMtx, &handptr->animMtx);
            sinf(tiltAngle / raiseTicks);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = gunGetReloadDipDepth(weapon) * (1.0f - cosf(reloadRaiseAngle));
            handptr->animMtx.m[3][2] = sinf(reloadRaiseAngle) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_WATCH_LOWER)
    {
        if ((handptr->stateTime >= WHEN_E_FLD890) || (handptr->handVisible == 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_WATCH_SWAP;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            watchLowerAngle = ((f32) handptr->stateTime * M_LN2F) / lowerTicks;
            handptr->useAnimMtx = 1;

            mtx4LoadXRotation(watchLowerAngle, &handptr->animMtx);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = (1.0f - cosf(watchLowerAngle)) * -60.0f;
            handptr->animMtx.m[3][2] = sinf(watchLowerAngle) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_WATCH_SWAP)
    {
        if ((handptr->stateFrames == 0) || (gunHandModelIsEmpty(hand) == 0))
        {
            gunSetWatchMenuWeapon(hand, handptr->weapon_next_weapon);
            weapon = gunGetShownWeapon(hand);
        }

        if (gunHandModelIsEmpty(hand) != 0)
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_WATCH_RAISE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            handptr->useAnimMtx = 1;
            mtx4LoadXRotation(M_LN2F, &handptr->animMtx);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = (1.0f - cosf(M_LN2F)) * -60.0f;
            handptr->animMtx.m[3][2] = sinf(M_LN2F) * 15.0f;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_WATCH_RAISE)
    {
        if ((handptr->stateFrames == 0) && (weapon < 0x21))
        {
            if (getPlayerCount() >= 2)
            {
                playerBodyGiveGun(hand);
            }
            gunReload(hand);
            g_CurrentPlayer->trigger_released = 0;
        }

        if ((handptr->stateTime >= WHEN_10_FLD890)
            || (get_ptr_weapon_model_header_line(weapon) == NULL)
            || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON) == 0)
            || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND) != 0))
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
        else
        {
            watchRaiseAngle = ((f32) (WHEN_10_FLD890 - handptr->stateTime) * M_LN2F) / raiseTicks;
            handptr->useAnimMtx = 1;
            mtx4LoadXRotation(watchRaiseAngle, &handptr->animMtx);
            handptr->animMtx.m[3][0] = 0.0f;
            handptr->animMtx.m[3][1] = (1.0f - cosf(watchRaiseAngle)) * -60.0f;
            handptr->animMtx.m[3][2] = sinf(watchRaiseAngle) * 15.0f;
        }
    }

    if ((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_BEGIN)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER))
    {
        slashTime = WEAPON_1P_ANIM_TIME(handptr->stateTime);

        if (((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
                || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH2_BEGIN))
                && (handptr->stateTime >= WHEN_11_FLD890_1))
        {
            sndPlaySfx(g_musicSfxBufferPtr, knife_throw_sounds[randomGetNext() % 3U], NULL);


            if (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_STRIKE;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_STRIKE;
            }
        }

        if ((handptr->weapon_action_state != GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER)
            && (handptr->weapon_action_state != GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER)
            && (handptr->stateTime >= WHEN_11_FLD890_2))
        {
            handptr->weapon_firing_status = 1;
            if ((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN) || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE))
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER;
            }
            else
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER;
                handptr->weapon_action_state = GUN_ANIM_STATE_KNIFE_SLASH2_RECOVER;
            }
        }

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_BEGIN)
            || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_STRIKE)
            || (handptr->weapon_action_state == GUN_ANIM_STATE_KNIFE_SLASH1_RECOVER))
        {
            slashKeyframes = knifeSlashKeyframes1;
        }
        else
        {
            slashKeyframes = knifeSlashKeyframes2;
        }

        if (gunSample1PTransform(slashKeyframes, slashTime, &handptr->animMtx, hand) != 0)
        {
            handptr->useAnimMtx = 1;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if ((handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_RECOVER)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_STRIKE)
        || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_RECOVER))
    {
        punchTime = WEAPON_1P_ANIM_TIME(handptr->stateTime);

        if ((handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_STRIKE) || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH1_RECOVER))
        {
            if (g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE)
            {
                punchKeyframes = sniperMeleeKeyframes1;
            }
            else
            {
                punchKeyframes = fistMeleeKeyframes1;
            }

            if ((handptr->weapon_action_state != GUN_ANIM_STATE_PUNCH1_RECOVER) && (handptr->stateTime >= WHEN_1E_FLD890))
            {
                handptr->weapon_firing_status = 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH1_RECOVER;
            }
        }
        else if ((handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_STRIKE) || (handptr->weapon_action_state == GUN_ANIM_STATE_PUNCH2_RECOVER))
        {
            if (g_CurrentPlayer->cur_item_weapon_getname == ITEM_SNIPERRIFLE)
            {
                punchKeyframes = sniperMeleeKeyframes2;
            }
            else
            {
                punchKeyframes = fistMeleeKeyframes2;
            }

            if ((handptr->weapon_action_state != GUN_ANIM_STATE_PUNCH2_RECOVER) && (handptr->stateTime >= WHEN_1E_FLD890))
            {
                handptr->weapon_firing_status = 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_PUNCH2_RECOVER;
            }
        }

        if (gunSample1PTransform(punchKeyframes, punchTime, &handptr->animMtx, hand) != 0)
        {
            handptr->useAnimMtx = 1;
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_GRENADE_THROW)
    {
        if (handptr->weapon_ammo_in_magazine > 0)
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
            if (gunSample1PTransform(grenadeThrowKeyframes, tempf, &handptr->animMtx, hand) != 0)
            {
                handptr->useAnimMtx = 1;
            }
            else
            {
                handptr->heldItemVisible = 0;
                handptr->weapon_firing_status = 1;
                handptr->weapon_ammo_in_magazine -= 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_GRENADE_RECOVER;
                handptr->stateTime = 0.0f;
                handptr->stateFrames = 0;
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_GRENADE_RECOVER)
    {
        tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
        if (gunSample1PTransform(timedMineThrowKeyframes, tempf, &handptr->animMtx, hand) != 0)
        {
            handptr->useAnimMtx = 1;
        }
        else
        {
            handptr->heldItemVisible = 1;
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_THROWKNIFE_DRAW)
    {
        if (handptr->weapon_ammo_in_magazine > 0)
        {
            if (handptr->triggerReleased != 0)
            {
                handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_THROW;
            }
            else
            {
                tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
                if (gunSample1PTransform(throwKnifeDrawBackKeyframes, tempf, &handptr->animMtx, hand) != 0)
                {
                    handptr->useAnimMtx = 1;
                }
                else if (gunSample1PTransform(throwKnifeReleaseKeyframes, 0.0f, &handptr->animMtx, hand) != 0)
                {
                    handptr->useAnimMtx = 1;
                }
                else
                {
                    handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_THROW;
                }
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_THROWKNIFE_THROW)
    {
        if (handptr->weapon_ammo_in_magazine > 0)
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
            if (gunSample1PTransform(throwKnifeDrawBackKeyframes, tempf, &handptr->animMtx, hand) != 0)
            {
                handptr->useAnimMtx = 1;
            }
            else
            {
                handptr->heldItemVisible = 0;
                handptr->weapon_firing_status = 1;
                handptr->weapon_ammo_in_magazine -= 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_THROWKNIFE_RECOVER;
                handptr->stateTime = 0.0f;
                handptr->stateFrames = 0;
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_THROWKNIFE_RECOVER)
    {
        tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
        if (gunSample1PTransform(throwKnifeReleaseKeyframes, tempf, &handptr->animMtx, hand) != 0)
        {
            handptr->useAnimMtx = 1;
        }
        else
        {
            handptr->heldItemVisible = 1;
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_MINE_PLACE)
    {
        if ((handptr->weapon_ammo_in_magazine > 0) || (weaponHasFlag(weapon, WEAPONSTATBITFLAG_CLICKY) != 0))
        {
            tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
            
            if (gunSample1PTransform(proxMineThrowKeyframes, tempf, &handptr->animMtx, hand) != 0)
            {
                handptr->useAnimMtx = 1;
            }
            else
            {
                handptr->heldItemVisible = 0;
                handptr->weapon_firing_status = 1;
                handptr->weapon_ammo_in_magazine -= 1;
                handptr->weapon_action_state = GUN_ANIM_STATE_MINE_RECOVER;
                handptr->stateTime = 0.0f;
                handptr->stateFrames = 0;
            }
        }
        else
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_MINE_RECOVER)
    {
        tempf = WEAPON_1P_ANIM_TIME(handptr->stateTime);
        if (gunSample1PTransform(remoteMineThrowKeyframes, tempf, &handptr->animMtx, hand) != 0)
        {
            handptr->useAnimMtx = 1;
        }
        else
        {
            handptr->heldItemVisible = 1;
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }

    if (handptr->weapon_action_state == GUN_ANIM_STATE_USE_ITEM)
    {
        if (weapon == ITEM_KEYANALYSERCASE)
        {
            if (handptr->stateFrames == 0)
            {
                gunAnalyzeGEKey();
            }
        }
        else if (weapon == ITEM_WEAPONCASE)
        {
            if (handptr->stateFrames == 0)
            {
                gunOpenWeaponCase();
            }
        }
        else if ((weapon == ITEM_BOMBDEFUSER)
            || (weapon == ITEM_DATATHIEF)
            || (weapon == ITEM_DOORDECODER)
            || (weapon == ITEM_EXPLOSIVEFLOPPY)
            || (weapon == ITEM_DATTAPE))
        {
            if (handptr->stateFrames == 0)
            {
                prop = propFindForInteract();

                if (prop != NULL)
                {
                    prop->obj->state |= 0x40;
                    coopNoteActivator();
                }
            }
        }
        else if ((weapon != ITEM_POLARIZEDGLASSES)
            && (weapon != ITEM_DARKGLASSES)
            && (weapon != ITEM_WATCHGEIGERCOUNTER)
            && (weapon != ITEM_WATCHMAGNETREPEL)
            && (weapon != ITEM_KEYCARD)
            && (weapon != ITEM_KEYYALE)
            && (weapon != ITEM_KEYBOLT)
            && (weapon != ITEM_SAFECRACKERCASE)
            && (weapon != ITEM_LOCKEXPLODER)
            && (weapon != ITEM_DOOREXPLODER)
            && (weapon != ITEM_CREDITCARD)
            && (weapon != ITEM_GASKEYRING)
            && (weapon == ITEM_BUNGEE))
        {
            // removed
        }
        else if (weapon == ITEM_PITONGUN
            || weapon == ITEM_GASKEYRING
            || weapon == ITEM_BUNGEE)
        {
            // removed
        }

        if (handptr->triggerReleased != 0)
        {
            handptr->weapon_action_state = GUN_ANIM_STATE_IDLE;
            handptr->stateTime = 0.0f;
            handptr->stateFrames = 0;
        }
    }
}

#undef WEAPON_1P_ANIM_TIME
#undef WHEN_1_CASE_GRENADELAUNCH_FLD890
#undef WHEN_1_CASE_GRENADE_FLD890
#undef WHEN_D_FLD890
#undef WHEN_5_SP188_INIT
#undef WHEN_5_SP188_MULTI
#undef WHEN_5_FLD8B0_SP
#undef WHEN_5_FLD8B0_MULTI
#undef WHEN_8_SP178_INIT
#undef WHEN_8_SP178_MULTI
#undef WHEN_A_FLD890
#undef WHEN_A_FLD8B0
#undef WHEN_C_FLD890
#undef WHEN_E_FLD890
#undef WHEN_10_FLD890
#undef WHEN_11_FLD890_1
#undef WHEN_11_FLD890_2
#undef WHEN_1E_FLD890


void gunAnalyzeGEKey(void)
{
    if (invHasGEKey())
    {
   	    HUDMESSAGEBOTTOM(langGet(getStringID(LGUN, GUN_STR_D8_ANALYZINGTHEGOLDENEYEKEY_LF))); //Analyzing the GoldenEye key...
    	g_CurrentPlayer->copiedgoldeneye = TRUE;
    	sndPlaySfx(g_musicSfxBufferPtr, KEY_ANALYSER_SFX, 0x0);
    	gunEquip(GUNRIGHT, ITEM_GOLDENEYEKEY);
    	gunEquip(GUNLEFT, ITEM_UNARMED);
  	}
  	else
  	{
	    HUDMESSAGEBOTTOM(langGet(getStringID(LGUN, GUN_STR_D9_YOUDONOTHAVETHEGOLDENEYEKEY_LF))); //You do not have the GoldenEye key.
	    gunEquipPreviousWeapons();
  	}
  	return;
}


s32 playerHasCopiedGEKey(void)
{
  s32 i;

  // Co-op: objectives are shared, so a copy made by either player counts.
  if (g_CoopMode)
  {
    for (i = 0; i < getPlayerCount(); i++)
    {
      if (g_playerPointers[i]->copiedgoldeneye)
      {
        return TRUE;
      }
    }
  }

  return g_CurrentPlayer->copiedgoldeneye;
}


void gunOpenWeaponCase(void)
{
  pickupAddAmmo(AMMO_KNIFE, 2, 0, 1);
  pickupAddAmmo(AMMO_GRENADE, 2, 0, 1);
  invAddItem(ITEM_SNIPERRIFLE);
  pickupPlayWeaponSound(ITEM_SNIPERRIFLE);
  pickupShowWeaponText(ITEM_SNIPERRIFLE);
  ammoGive(sniperrifle_stats.AmmoType, ammoGetHeld(sniperrifle_stats.AmmoType) + sniperrifle_stats.MagSize);
  invRemoveItemByID(ITEM_WEAPONCASE);
  gunEquip(GUNRIGHT,ITEM_SNIPERRIFLE);
  gunEquip(GUNLEFT,ITEM_UNARMED);
}


f32 gunGetNoise(GUNHAND hand)
{
    return g_CurrentPlayer->hands[hand].noise;
}


void gunTickNoise(void)
{
    enum ITEM_IDS weapon_id_right;
    enum ITEM_IDS weapon_id_left;
    f32 noise_reduction;
    WeaponStats *item_right_stats;
    WeaponStats *item_left_stats;
    f32 noise_reduction_max;

    weapon_id_right = gunGetWeapon(GUNRIGHT);
    weapon_id_left = gunGetWeapon(GUNLEFT);
    item_right_stats = weaponGetStats(weapon_id_right);
    item_left_stats = weaponGetStats(weapon_id_left);

    if (weapon_id_right != ITEM_UNARMED && gunGetFiringStatus(GUNRIGHT))
    {
        g_CurrentPlayer->hands[GUNRIGHT].noise += item_right_stats->NoiseIncreasePerShot;

        if (item_right_stats->LoudnessMax < g_CurrentPlayer->hands[GUNRIGHT].noise)
        {
            g_CurrentPlayer->hands[GUNRIGHT].noise = item_right_stats->LoudnessMax;
        }
    }

    if (weapon_id_left != ITEM_UNARMED && gunGetFiringStatus(GUNLEFT))
    {
        g_CurrentPlayer->hands[GUNLEFT].noise += item_left_stats->NoiseIncreasePerShot;

        if (item_left_stats->LoudnessMax < g_CurrentPlayer->hands[GUNLEFT].noise)
        {
            g_CurrentPlayer->hands[GUNLEFT].noise = item_left_stats->LoudnessMax;
        }
    }

    noise_reduction = (item_right_stats->NoiseIncreasePerShot * g_GlobalTimerDelta) / (item_right_stats->NoiseDecayLinearTime * 60.0f);
    noise_reduction_max = ((g_CurrentPlayer->hands[GUNRIGHT].noise - item_right_stats->LoudnessMin) * g_GlobalTimerDelta) / (item_right_stats->NoiseDecayScaledTime * 60.0f);

    if (noise_reduction < noise_reduction_max)
    {
        noise_reduction = noise_reduction_max;
    }

    g_CurrentPlayer->hands[GUNRIGHT].noise -= noise_reduction;

    if (g_CurrentPlayer->hands[GUNRIGHT].noise < item_right_stats->LoudnessMin)
    {
        g_CurrentPlayer->hands[GUNRIGHT].noise = item_right_stats->LoudnessMin;
    }

    noise_reduction = (item_left_stats->NoiseIncreasePerShot * g_GlobalTimerDelta) / (item_left_stats->NoiseDecayLinearTime * 60.0f);
    noise_reduction_max = ((g_CurrentPlayer->hands[GUNLEFT].noise - item_left_stats->LoudnessMin) * g_GlobalTimerDelta) / (item_left_stats->NoiseDecayScaledTime * 60.0f);

    if (noise_reduction < noise_reduction_max)
    {
        noise_reduction = noise_reduction_max;
    }

    g_CurrentPlayer->hands[GUNLEFT].noise -= noise_reduction;

    if (g_CurrentPlayer->hands[GUNLEFT].noise < item_left_stats->LoudnessMin)
    {
        g_CurrentPlayer->hands[GUNLEFT].noise = item_left_stats->LoudnessMin;
    }
}


/**
 * Returns true if the hand has a melee weapon or has ammo in the magazine.
 */
s32 gunCanUseWeapon(enum GUNHAND hand)
{
    return (weaponGetAmmoType(gunGetWeapon(hand)) == 0)
        || (g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine > 0);
}


/**
 * Perfect Dark method bgunTickGameplay.
 *
 * Handles logic for single gun and dual wield trigger presses.
 * Calls updates to first person gun animations, gun model loading, noise to AI, and updating color from collision tiles.
 * Also handles the Watch Magnet Attract hum noise.
*/
void gunTickGameplay(s32 triggerOn)
{
    struct gun_trigger_state trigger_state;
    enum ITEM_IDS weapon_id_right;
    enum ITEM_IDS weapon_id_left;
    enum GUNHAND hand = GUNLEFT;
    rgba_u8 weapon_color;

    trigger_state = (struct gun_trigger_state){};

    // Save previous trigger state.
    g_CurrentPlayer->prev_trigger_down = g_CurrentPlayer->trigger_down;

    // Save raw trigger state.
    g_CurrentPlayer->trigger_down = triggerOn;

    if ((g_CurrentPlayer->trigger_down == 0) && (g_CurrentPlayer->prev_trigger_down != 0))
    {
        g_CurrentPlayer->trigger_released = 1;
    }

    // Z button pressed this frame.
    if (g_CurrentPlayer->trigger_down != 0)
    {
        weapon_id_right = gunGetWeapon(GUNRIGHT);
        weapon_id_left = gunGetWeapon(GUNLEFT);

        g_CurrentPlayer->z_trigger_timer += g_ClockTimer;

        // Dual wielding.
        if ((weapon_id_right != ITEM_UNARMED) && (weapon_id_left != ITEM_UNARMED))
        {
            // Both guns prefer to take turns firing in dual wield.
            if ((weaponHasFlag(weapon_id_right, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0) && (weaponHasFlag(weapon_id_left, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0))
            {
                // Trigger has been held longer than 20 ticks on NTSC (24 on PAL).
                if (g_CurrentPlayer->z_trigger_timer > DUAL_WIELD_TRIGGER_SWAP_TICKS)
                {
                    // 'hand' still has its default value here, which behaves like trigger-on.
                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = hand;

                    // If the gun in the other hand is usable or has been held for any amount of time, depress its trigger as well.
                    if (gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) || g_CurrentPlayer->hands[1 - g_CurrentPlayer->current_trigger_hand].weapon_hold_time)
                    {
                        trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 1;
                    }
                }
                // Z has been held for less than or equal to 20 ticks on NTSC (24 on PAL).
                else
                {
                    if ((g_CurrentPlayer->prev_trigger_down == 0) &&
                        ((gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) != 0) || (gunCanUseWeapon(g_CurrentPlayer->current_trigger_hand) == 0)))
                    {
                        g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
                    }

                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
                    trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
                }
            }
            /**
             * One gun prefers to take turns firing in dual wield.
             * This doesn't happen much in the vanilla US version with the notable
             * exception of equipping Xenia's RC-P90 and Grenade Launcher in Jungle
            */
            else if ((weaponHasFlag(weapon_id_right, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0) || (weaponHasFlag(weapon_id_left, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) != 0))
            {
                // Z has been held more than 30 ticks on NTSC (36 for PAL), depress trigger on both guns.
                if (g_CurrentPlayer->z_trigger_timer > DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS)
                {
                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = hand;

                    if ((gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) != 0) || g_CurrentPlayer->hands[1 - g_CurrentPlayer->current_trigger_hand].weapon_hold_time != 0)
                    {
                        trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 1;
                    }
                }
                // Before the hold threshold (30 ticks NTSC or 36 PAL), prefer the hand whose weapon uses alternating dual wield fire.
                else
                {
                    hand = weaponHasFlag(weapon_id_right, WEAPONSTATBITFLAG_DUAL_WIELD_ALTERNATING_FIRE) ? GUNRIGHT : GUNLEFT;

                    if (gunCanUseWeapon(hand) != 0 || g_CurrentPlayer->hands[hand].weapon_hold_time != 0)
                    {
                        g_CurrentPlayer->current_trigger_hand = hand;
                    }
                    else
                    {
                        if ((gunCanUseWeapon(1 - hand) != 0) || g_CurrentPlayer->hands[1 - hand].weapon_hold_time != 0)
                        {
                            g_CurrentPlayer->current_trigger_hand = 1 - hand;
                        }
                        else
                        {
                            g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
                        }
                    }

                    trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
                    trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
                }
            }
            /**
             * Neither weapon uses alternating dual wield fire.
             * Once the hold threshold is exceeded, allow the off-hand to become active too.
             */
            else if (g_CurrentPlayer->z_trigger_timer > DUAL_WIELD_SINGLE_TRIGGER_SWAP_TICKS)
            {
                trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = hand;

                if (gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) || g_CurrentPlayer->hands[1 - g_CurrentPlayer->current_trigger_hand].weapon_hold_time)
                {
                    trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 1;
                }
            }
            /**
             * Neither weapon uses alternating dual wield fire.
             * On a fresh Z press, switch lead hands if the other hand is usable or the current hand cannot be used.
             * The lead hand continues being the lead hand.
             */
            else
            {
                if ((g_CurrentPlayer->prev_trigger_down == 0) &&
                    ((gunCanUseWeapon(1 - g_CurrentPlayer->current_trigger_hand) != 0) || (gunCanUseWeapon(g_CurrentPlayer->current_trigger_hand) == 0)))
                {
                    g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
                }

                trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
                trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
            }
        }
        // Not dual wielding.
        else
        {
            if ((gunGetWeapon(g_CurrentPlayer->current_trigger_hand) == ITEM_UNARMED) && (gunGetWeapon(1 - g_CurrentPlayer->current_trigger_hand) != ITEM_UNARMED))
            {
                g_CurrentPlayer->current_trigger_hand = 1 - g_CurrentPlayer->current_trigger_hand;
            }

            trigger_state.triggerOn[g_CurrentPlayer->current_trigger_hand] = 1;
            trigger_state.triggerOn[1 - g_CurrentPlayer->current_trigger_hand] = 0;
        }
    }
    // Z button not pressed. Reset the trigger timer.
    else
    {
        g_CurrentPlayer->z_trigger_timer = 0;
    }

    gunTickHandState(0, trigger_state.triggerOn[0]); // Right hand
    gunTickHandState(1, trigger_state.triggerOn[1]); // Left hand
    gunLoadHandModel(0);
    gunLoadHandModel(1);
    gunTickNoise();

    if (g_CurrentPlayer->resetshadecol)
    {
        objGetTileShading(currentPlayerGetProp(), &g_CurrentPlayer->tileColor);
        g_CurrentPlayer->resetshadecol = FALSE;
    }
    else
    {
        objGetTileShading(currentPlayerGetProp(), &weapon_color);
        objUpdateShading(&g_CurrentPlayer->tileColor, &weapon_color);
    }

    invIncrementHeldTime(gunGetWeapon(GUNRIGHT), gunGetWeapon(GUNLEFT));


    if (g_CurrentPlayer->magnetattracttime >= 0)
    {
        struct hand *hand_right = &g_CurrentPlayer->hands[0];

        g_CurrentPlayer->magnetattracttime += g_ClockTimer;

        if (g_CurrentPlayer->magnetattracttime < WATCH_SOUND_DURATION_TICKS)
        {
            // Start or restart the hum sound if needed
            if (hand_right->audioHandle == NULL
                || sndGetPlayingState(hand_right->audioHandle) == 0)
            {
                if (lvGetControlsLocked() == 0)
                {
                    sndPlaySfx(
                        g_musicSfxBufferPtr,
                        MAGNETIC_HUM_SFX,
                        &hand_right->audioHandle);
                }
            }
        }
        else
        {
            g_CurrentPlayer->magnetattracttime = -1;

            if (hand_right->audioHandle != NULL)
            {
                if (sndGetPlayingState(hand_right->audioHandle) != 0)
                {
                    sndDeactivate(hand_right->audioHandle);
                }
            }
        }
    }
}


void gunSetAimPos(coord3d *pos)
{
  g_CurrentPlayer->hands[GUNRIGHT].aimPos.x = gunGetDualOffset(GUNRIGHT) + pos->x;
  g_CurrentPlayer->hands[GUNRIGHT].aimPos.y = pos->y;
  g_CurrentPlayer->hands[GUNRIGHT].aimPos.z = pos->z;

  g_CurrentPlayer->hands[GUNLEFT].aimPos.x = gunGetDualOffset(GUNLEFT) + pos->x;
  g_CurrentPlayer->hands[GUNLEFT].aimPos.y = pos->y;
  g_CurrentPlayer->hands[GUNLEFT].aimPos.z = pos->z;

}


void gunSetTracerTarget(coord3d* pos)
{
    g_CurrentPlayer->hands[GUNLEFT].item_related.x = g_CurrentPlayer->hands[GUNRIGHT].item_related.x = pos->x;
    g_CurrentPlayer->hands[GUNLEFT].item_related.y = g_CurrentPlayer->hands[GUNRIGHT].item_related.y = pos->y;
    g_CurrentPlayer->hands[GUNLEFT].item_related.z = g_CurrentPlayer->hands[GUNRIGHT].item_related.z = pos->z;
}


void gunUpdateCrosshair(f32 turn_x, f32 turn_y, f32 guncrossdamp, f32 gunaimdamp)
{
    s32 i;
    f32 screen_width;
    f32 screen_height;
    coord3d coords;

    screen_width = camGetScreenWidth();
    screen_height = camGetScreenHeight();

    if (guncrossdamp != g_CurrentPlayer->guncrossdamp)
    {
        g_CurrentPlayer->crosshair_x_pos = (g_CurrentPlayer->crosshair_x_pos * (1.0f - g_CurrentPlayer->guncrossdamp)) / (1.0f - guncrossdamp);
        g_CurrentPlayer->crosshair_y_pos = (g_CurrentPlayer->crosshair_y_pos * (1.0f - g_CurrentPlayer->guncrossdamp)) / (1.0f - guncrossdamp);
        g_CurrentPlayer->guncrossdamp = guncrossdamp;
    }

    if (gunaimdamp != g_CurrentPlayer->gunaimdamp)
    {
        g_CurrentPlayer->gun_azimuth_angle = (g_CurrentPlayer->gun_azimuth_angle * (1.0f - g_CurrentPlayer->gunaimdamp)) / (1.0f - gunaimdamp);
        g_CurrentPlayer->gun_azimuth_turning = (g_CurrentPlayer->gun_azimuth_turning * (1.0f - g_CurrentPlayer->gunaimdamp)) / (1.0f - gunaimdamp);
        g_CurrentPlayer->gunaimdamp = gunaimdamp;
    }

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->crosshair_x_pos = (g_CurrentPlayer->crosshair_x_pos * guncrossdamp) + turn_x;
        g_CurrentPlayer->crosshair_y_pos = (g_CurrentPlayer->crosshair_y_pos * guncrossdamp) + turn_y;
    }

    g_CurrentPlayer->crosshair_angle.f[0] = (g_CurrentPlayer->crosshair_x_pos * (1.0f - guncrossdamp) * screen_width * 0.5f) + (screen_width * 0.5f);
    g_CurrentPlayer->crosshair_angle.f[1] = (g_CurrentPlayer->crosshair_y_pos * (1.0f - guncrossdamp) * screen_height * 0.5f) + (screen_height * 0.5f);

    if (g_CurrentPlayer->crosshair_angle.f[0] < 3.0f)
    {
        g_CurrentPlayer->crosshair_angle.f[0] = 3.0f;
    }
    else if ((screen_width - 4.0f) < g_CurrentPlayer->crosshair_angle.f[0])
    {
        g_CurrentPlayer->crosshair_angle.f[0] = screen_width - 4.0f;
    }

    if (g_CurrentPlayer->crosshair_angle.f[1] < 3.0f)
    {
        g_CurrentPlayer->crosshair_angle.f[1] = 3.0f;
    }
    else if ((screen_height - 4.0f) < g_CurrentPlayer->crosshair_angle.f[1])
    {
        g_CurrentPlayer->crosshair_angle.f[1] = (screen_height - 4.0f);
    }

    g_CurrentPlayer->crosshair_angle.f[0] += camGetScreenLeft();
    g_CurrentPlayer->crosshair_angle.f[1] += camGetScreenTop();

    for (i = 0; i < g_ClockTimer; i++)
    {
        g_CurrentPlayer->gun_azimuth_angle = (g_CurrentPlayer->gun_azimuth_angle * gunaimdamp) + turn_x;
        g_CurrentPlayer->gun_azimuth_turning = (g_CurrentPlayer->gun_azimuth_turning * gunaimdamp) + turn_y;
    }

    g_CurrentPlayer->gunAimScreenPos.x = (g_CurrentPlayer->gun_azimuth_angle * (1.0f - gunaimdamp) * screen_width * 0.5f) + (screen_width * 0.5f);
    g_CurrentPlayer->gunAimScreenPos.y = (g_CurrentPlayer->gun_azimuth_turning * (1.0f - gunaimdamp) * screen_height * 0.5f) + (screen_height * 0.5f);

    g_CurrentPlayer->gunAimScreenPos.x += camGetScreenLeft();
    g_CurrentPlayer->gunAimScreenPos.y += camGetScreenTop();

    camScreenToViewDir(&g_CurrentPlayer->gunAimScreenPos, &coords, 1000.0f);
    gunSetAimPos(&coords);
}


void gunUpdateCrosshairSwivel(f32 turn_x, f32 turn_y, f32 max_aim_lock_speed)
{
    f32 aim_lock_speed;

#if defined(VERSION_US)
    aim_lock_speed = weaponGetStats(gunGetWeapon(GUNRIGHT))->AimLockSpeed;
#elif defined(VERSION_EU)
    aim_lock_speed = weaponGetStats(gunGetWeapon(GUNRIGHT))->CrosshairSpeed;
#endif

    if (aim_lock_speed < max_aim_lock_speed)
    {
        aim_lock_speed = max_aim_lock_speed;
    }

    gunUpdateCrosshair(turn_x, turn_y, max_aim_lock_speed, aim_lock_speed);
}


void gunUpdateCrosshairManual(f32 turn_x, f32 turn_y)
{
    WeaponStats * item_stats;
    f32 guncrossdamp;
    f32 gunaimdamp;

    item_stats = weaponGetStats(gunGetWeapon(GUNRIGHT));

#if defined(VERSION_US)
    guncrossdamp = item_stats->CrosshairSpeed;
    gunaimdamp = item_stats->AimLockSpeed;
#elif defined(VERSION_EU)
    guncrossdamp = 0.7651f;
    gunaimdamp = item_stats->CrosshairSpeed;
#endif

    gunUpdateCrosshair(turn_x, turn_y, guncrossdamp, gunaimdamp);
}


void gunGetCrosshairPos(f32* horizontal_angle, f32* vertical_angle) {
	*horizontal_angle = g_CurrentPlayer->crosshair_angle.f[0];
	*vertical_angle = g_CurrentPlayer->crosshair_angle.f[1];
}


// Screen width over height, for spreading shots across the crosshair
#if VERSION_EU
static const f32 g_GunScreenAspectRatio = 20.0f / 17.0f;
#else
static const f32 g_GunScreenAspectRatio = 4.0f / 3.0f;
#endif


void gunGetAimRay(coord3d *zeropos, coord3d *vec)
{
    zeropos->x = 0.0f;
    zeropos->y = 0.0f;
    zeropos->z = 0.0f;

    camScreenToViewDir(&g_CurrentPlayer->crosshair_angle, vec, 1.0f);
}


/*
* This function computes the angle the player's bullets are fired at
*/
void gunGetShotDir(coord3d* origin, coord3d* result, enum GUNHAND hand)
{
    coord2d crosspos;
    f32 inaccuracy;
    f32 scaledspread;
    f32 randfactor;

    inaccuracy = weaponGetStats(gunGetWeapon(hand))->Inaccuracy;
    if ((weaponHasFlag(gunGetShownWeapon(hand), WEAPONSTATBITFLAG_FIRST_SHOT_ACCURACY) != 0) && (g_CurrentPlayer->hands[hand].volley == 1))
    {
        // Single shots are four times more accurate
        inaccuracy *= 0.25f;
    }

    scaledspread = (120.0f * inaccuracy) / viGetFovY();

    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.x = g_CurrentPlayer->crosshair_angle.f[0] + randfactor * scaledspread * camGetScreenWidth() * g_GunScreenAspectRatio
        / (camGetPerspAspect() * 320.0f);

    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.y =  g_CurrentPlayer->crosshair_angle.f[1] + randfactor * scaledspread * camGetScreenHeight()
        / (PAL ? (f32)(SCREEN_HEIGHT_272) : (f32)(SCREEN_HEIGHT_240));

    origin->x = 0.0f;
    origin->y = 0.0f;
    origin->z = 0.0f;

    // Result is a normalized vector describing the path the bullet will follow
    // Can be used to compute x,y,z displacement off the center of the screen if done for a projectile
    camScreenToViewDir(&crosspos, result, 1.0f);
}


