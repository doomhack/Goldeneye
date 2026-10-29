/**
 * The hands each frame: the hand state machine (gunTickHandState), placing and drawing
 * the gun models, beams and tracers, hit sounds, the crosshair and aim, ejected casings,
 * ammo, the ammo HUD and sight, per-player stats, and firing shots into the world
 * (shotFireHands). See docs/subsystems/weapons.md.
 */
#include <ultra64.h>
#include <assert.h>
#include <bondaicommands.h>
#include "game/world/bg.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/prop/explosion.h"
#include "game/prop/glass.h"
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

s32 ammoGetHeld(AMMOTYPE ammotype);


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

extern f32 g_TankShellSpeed;
extern u32 watchControllerButtonBases[];
extern GunModelFileRecord gitem_structs[];
extern Lights1 g_WeaponEnvmapLight;
extern ALSoundState *g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];
extern u16 ricochet_sounds_small[20];
extern u16 punch_sounds[3];
extern u16 bullet_flesh_sounds[2];
extern u16 laser_ricochet_sounds[2];
extern u16 ricochet_sounds_large[36];
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

void gunCreateBeamForHand(enum GUNHAND hand);
void gunGetShotDir(coord3d *origin, coord3d *result, enum GUNHAND hand);
void gunLaunchProjectileFromPlayer(ObjectRecord *obj, coord3d *targetpos, Mtxf *mtx, coord3d *velocity, Mtxf *tumble);
s32 gunSample1PTransform(Weapon1PTransformKeyframe *keyframes, f32 time, Mtxf *matrix, GUNHAND hand);
void gunAnalyzeGEKey(void);
void gunOpenWeaponCase(void);
struct ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS weapon);
s32 gunGetHeldAmmo(enum GUNHAND hand);
s32 weaponGetAmmoType(ITEM_IDS weapon);
f32 gunSetHorizontalOffset(GUNHAND hand);
f32 gunGetMaxHingeAngle(GUNHAND hand);
void gunSetWatchMenuWeapon(GUNHAND hand, ITEM_IDS weaponnum_watchmenu);
void gunStartBoltRecoil(GUNHAND hand);
void gunSetHeldItemVisible(Model *model, s32 val);
void gunReload(enum GUNHAND hand);
void gunEjectCasings(GUNHAND handnum, f32 floor_y_pos);
void *dynAllocateVertices(s32 count);
Mtx *dynAllocateMatrix(void);
void camViewDeltaToScreen(coord3d *in, f32 divisor, coord3d *out);

#if !defined(VERSION_EU)
void gunFreeRocket(enum GUNHAND hand)
{
    struct hand* hand_ptr;
    ObjectRecord* hand_obj_record;

    hand_ptr = &g_CurrentPlayer->hands[hand];
    hand_obj_record = hand_ptr->rocket;

    if (hand_obj_record != NULL)
    {
        objFreePermanently(hand_obj_record, 1);
        hand_ptr->rocket = NULL;
    }
}


extern f32 g_ThrowVelocityScale;

void gunFireTankShell(s32 handnum)
{
    WeaponObjRecord *obj;
    struct hand *hand;
    Mtxf identitymtx;
    coord3d velocity;
    ObjectRecord *tankobj;
    coord3d unscaledvelocity;
    Mtxf shellmtx;
    coord3d screenpos;
    coord3d aimdir;
    PropRecord *playerprop;
    coord3d *prevplayerpos;
    ITEM_IDS weaponid;
    coord3d spawnpos;
    PropRecord *tankprop;

    hand = &g_CurrentPlayer->hands[handnum];

    playerprop = currentPlayerGetProp();
    prevplayerpos = playerGetPrevPos();
    weaponid = gunGetWeapon(handnum);

    mtx4LoadIdentity(&identitymtx);

    if (weaponid == ITEM_TANKSHELLS) 
    {
        tankprop = playerGetTank();


        if ((tankprop != NULL) && (tankprop->flags & TANK_RUN_STATE_RUNNING)) 
        {
            tankGetTurretDir(&aimdir);
        } 
        else 
        {
            gunGetAimRay(&screenpos, &aimdir);
            mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &aimdir);
        }

        velocity.x = aimdir.x * g_TankShellSpeed;
        velocity.y = aimdir.y * g_TankShellSpeed;
        velocity.z = aimdir.z * g_TankShellSpeed;

        if (g_ClockTimer > 0) {
            velocity.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
            velocity.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
            velocity.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
        }

        if ((tankprop != NULL) && (tankprop->flags & TANK_RUN_STATE_RUNNING)) 
        {
            tankobj = tankprop->obj;
            spawnpos.x = tankobj->model->render_pos[4].pos.m[3][0];
            spawnpos.y = tankobj->model->render_pos[4].pos.m[3][1];
            spawnpos.z = tankobj->model->render_pos[4].pos.m[3][2];

            mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &spawnpos);
        } 
        else 
        {
            spawnpos.x = playerprop->pos.x;
            spawnpos.y = playerprop->pos.y;
            spawnpos.z = playerprop->pos.z;
        }


        explosionStartShake();
    } 
    else 
    {
        gunGetShotDir(&screenpos, &aimdir, handnum);
        mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &aimdir);

        spawnpos.x = hand->muzzlePos.x;
        spawnpos.y = hand->muzzlePos.y;
        spawnpos.z = hand->muzzlePos.z;


        unscaledvelocity.x = aimdir.x * g_ThrowVelocityScale;
        unscaledvelocity.y = aimdir.y * g_ThrowVelocityScale;
        unscaledvelocity.z = aimdir.z * g_ThrowVelocityScale;

        velocity.x = unscaledvelocity.x * g_GlobalTimerDelta;
        velocity.y = unscaledvelocity.y * g_GlobalTimerDelta;
        velocity.z = unscaledvelocity.z * g_GlobalTimerDelta;

        if (g_ClockTimer > 0) 
        {
            velocity.x += (playerprop->pos.x - prevplayerpos->x) / g_GlobalTimerDelta;
            velocity.y += (playerprop->pos.y - prevplayerpos->y) / g_GlobalTimerDelta;
            velocity.z += (playerprop->pos.z - prevplayerpos->z) / g_GlobalTimerDelta;
        }
    }

    mtx4Copy(&g_CurrentPlayer->hands[handnum].gunWorldMtx, &shellmtx);

    shellmtx.m[3][0] = 0.0f;
    shellmtx.m[3][1] = 0.0f;
    shellmtx.m[3][2] = 0.0f;

    if (hand->rocket != NULL) 
    {
        obj = (WeaponObjRecord *) hand->rocket;
        hand->firedrocket = 1;
    } 
    else 
    {
        obj = (WeaponObjRecord *) weaponCreateFromModel(PROP_CHRROCKET, ITEM_ROCKETROUND);
    }

    if (obj == NULL) 
    {
        return;
    }

    obj->timer = -1;
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
    obj->runtime_bitflags |= playerGetCurrentNum() << RUNTIMEBITSHIFT_OWNER;

    gunLaunchProjectileFromPlayer(obj, &spawnpos, &shellmtx, &velocity, &identitymtx);

    if (obj->runtime_bitflags & RUNTIMEBITFLAG_00000080)
    {
        obj->projectile->flags |= PROJECTILEFLAG_LAUNCHING;

        if (weaponid != ITEM_TANKSHELLS)
        {
            obj->projectile->flags |= PROJECTILEFLAG_00000020;
            obj->projectile->riseY = obj->runtime_pos.y;
            obj->projectile->riseSpeed = obj->projectile->speed.y;
            obj->projectile->accel.x = unscaledvelocity.x;
            obj->projectile->accel.y = unscaledvelocity.y;
            obj->projectile->accel.z = unscaledvelocity.z;
            obj->projectile->refreshrate = THROWN_ITEM_REFRESH_RATE;

            if (obj->projectile->sounds[0] == NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, 1, &obj->projectile->sounds[0]);
            } 
            else if (obj->projectile->sounds[1] == NULL)
            {
                sndPlaySfx(g_musicSfxBufferPtr, 1, &obj->projectile->sounds[1]);
            }
        }
    }
}


/**
 * g_ThrowVelocityScale belongs to gunFireTankShell, but must sit here
 * immediately ahead of this function's pool for that function to match.
 */
#endif

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

        // Ideally this should be something like gSPMatrix(gdl++ or cmd++, osVirtualToPhysical(mtx), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH) but I can't get these or similar forms to match
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
 
            if (item == ITEM_ROCKETLAUNCH) 
            {
                monitorSetImage(&g_UnknownAnimController, g_ImageCrosshair);
                gdl = monitorTickAnimation(model, handptr->gunmodel.obj->Switches[16], &g_UnknownAnimController, gdl, 0, 4);
            } 
            else 
            {
                gdl = monitorTickAnimation(model, handptr->gunmodel.obj->Switches[16], &g_TaserAnimController, gdl, 0, 1);
            }
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


Gfx *watchRenderItem(Gfx *gdl, ITEM_IDS itemid, Mtxf *mtx, s32 alpha, s32 colour)
{
    ModelRenderData renderdata = {.zbufferenabled = TRUE, .flags = 3};
    Model model = {};
    u32 rwbuf[0x80 / sizeof(u32)];
    Mtxf tmp;
    Mtxf *matrices;
    union ModelRwData *rwdata;
    s32 i;
    s32 j;
    ModelFileHeader *bodymodel;

    if ((itemid == ITEM_TRIGGER) || (itemid == ITEM_WATCHLASER))
    {
        itemid = ITEM_WATCHMAGNETATTRACT;
    }

    gunSetWatchMenuWeapon(GUNRIGHT, itemid);

    if ((!gunHandModelIsEmpty(GUNRIGHT)) || (!gunGetHandModelItem(GUNRIGHT)))
    {
        return gdl;
    }

    bodymodel = &g_CurrentPlayer->handModelHeader[GUNRIGHT];

    if (!get_ptr_weapon_model_header_line(itemid))
    {
        goto earlyreturn;
    }

    if (weaponHasFlag(itemid, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_MENU))
    {
        goto earlyreturn;
    }

    matrices = dynAllocate(bodymodel->numMatrices << 6);
    
    for (i = 0; i < bodymodel->numMatrices; i++)
    {
        mtx4LoadIdentity(&matrices[i]);
    }

    i = 0;
    model.render_pos = (RenderPosView *)matrices;
    modelCalculateRwDataLen(bodymodel);
    modelInit(&model, bodymodel, rwbuf);
    gunSetFirstPersonPartsVisible(&model, 0);
    gunSetHeldItemVisible(&model, 1);

    if (bodymodel->Switches[1] != NULL)
    {
        rwdata = modelGetNodeRwData(&model, bodymodel->Switches[1]);

        if (rwdata != 0)
        {
            rwdata->Switch.visible = 0;
        }
    }

    mtx4Copy(mtx, matrices);
  
    if (bodymodel->Skeleton == (&skeleton_gun_revolver))
    {
        if (bodymodel->Switches[4] != NULL)
        {
            mtx4LoadTranslation((coord3d *) bodymodel->Switches[4]->Data, &tmp);
            mtx4Mult(mtx, &tmp, &matrices[3]);
        }
        if (bodymodel->Switches[5] != NULL)
        {
            mtx4LoadTranslation((coord3d *) bodymodel->Switches[5]->Data, &tmp);
            mtx4Mult(mtx, &tmp, &matrices[4]);
        }
    }

    if (bodymodel->Switches[6] != NULL)
    {
        coord3d *pos;
        s32 index;

        pos = (coord3d *) bodymodel->Switches[6]->Data;
        index = modelFindNodeMtxIndex(bodymodel->Switches[6], 0);
        mtx4LoadTranslation(pos, &tmp);
        mtx4Mult(mtx, &tmp, &matrices[index]);
    }

    if (bodymodel->Switches[7] != NULL)
    {
        coord3d *pos;
        s32 index;

        pos = (coord3d *) bodymodel->Switches[7]->Data;
        index = modelFindNodeMtxIndex(bodymodel->Switches[7], 0);
        mtx4LoadTranslation(pos, &tmp);
        mtx4Mult(mtx, &tmp, &matrices[index]);
    }

    if (bodymodel->numSwitches >= 19)
    {
        for (j = 0; j != 20; j += 4)
        {
            if ((*((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x48))) != NULL)
            {
                rwdata = modelGetNodeRwData(&model, *((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x48)));

                if (rwdata != NULL)
                {
                    rwdata->Switch.visible = 1;
                }
            }

            if ((*((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x5c))) != NULL)
            {
                rwdata = modelGetNodeRwData(&model, *((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x5c)));

                if (rwdata != NULL)
                {
                    rwdata->Switch.visible = 1;
                }
            }
        }
    }

    modelUpdateNodeRelations(&model);

    if ((((((itemid == ITEM_GOLDENGUN) || (itemid == ITEM_RUGER)) || (itemid == ITEM_KNIFE)) || (itemid == ITEM_THROWKNIFE)) || (itemid == ITEM_SILVERWPPK)) || (itemid == ITEM_GOLDWPPK))
    {
        gSPSetLights1(gdl++, g_WeaponEnvmapLight);
        gSPLookAt(gdl++, camGetLookAt());
    }

    if (bodymodel->numSwitches >= 17)
    {
        if (bodymodel->Switches[16] != NULL)
        {
            rwdata = modelGetNodeRwData(&model, bodymodel->Switches[17]);

            if (rwdata != 0)
            {
                rwdata->Switch.visible = 0;
            }
        }
    }

    renderdata.gdl = gdl;

    if (alpha >= 0xff)
    {
        renderdata.PropType = PROP_TYPE_WEAPON;
        renderdata.envcolour.word = colour;
    }
    else
    {
        renderdata.PropType = PROP_TYPE_PLAYER;
        renderdata.envcolour.word = alpha;
        renderdata.fogcolour.word = colour;
    }

    renderdata.zbufferenabled = FALSE;
    modelRender(&renderdata, &model);
    gdl = renderdata.gdl;
    mtxPushUnitWorldScale();
    j = 0;

    if (bodymodel->numMatrices > 0)
    {
        do
        {
            mtx4Copy((Mtxf *) (((u8 *) model.render_pos) + j), &tmp);
            mtxF2L(&tmp, (Mtxf *) ((i << 6) + (u8 *) model.render_pos));
            i++;
            j += 0x40;
        }
        while (i < bodymodel->numMatrices);
    }

    mtxPopWorldScale();

earlyreturn:
    return gdl;

}


/**
 * The matrix for one button of the watch menu's controller model:
 * out = m5 * translate(pos) * m3 * m2 * m1 * translate(base).
 */
void watchControllerButtonMtx(coord3d *base, Mtxf *m1, Mtxf *m2, Mtxf *m3, coord3d *pos, Mtxf *m5, Mtxf *out)
{
    Mtxf translation;

    mtx4LoadTranslation(base, out);
    mtx4MultInPlace(m1, out);
    mtx4MultInPlace(m2, out);
    mtx4MultInPlace(m3, out);
    mtx4LoadTranslation(pos, &translation);
    mtx4MultInPlace(&translation, out);
    mtx4MultInPlace(m5, out);
}


Gfx* watchRenderController(Gfx* gdl, Mtxf* basemtx, s32 envcolour, bool animatebuttons, WatchContButtonPositions* buttonpositions, s8* contpadnum)
{
    ModelRenderData renderdata;
    Model modelstack = {};
    s32 i;
    f32 angle;
    u32 rwdata[26];
    Mtxf nodeMtx;
    Mtxf rotMtx;
    struct ModelFileHeader* objheader;
    coord3d* position;
    Mtxf* matrices;
    ModelNode* node;
    Mtxf buttonMtx;
    Mtxf transMtx;
    Mtxf tmpMtx;
    Mtxf pressMtx;
    Mtxf tiltMtx;
    Mtxf viewMtx;
    Mtxf identityMtx;
    coord3d coord_node2;
    coord3d coord_node11_pos;
    coord3d coord_node11_base;
    coord3d coord_node4_pos;
    coord3d coord_node4_base;
    coord3d coord_node5_pos;
    coord3d coord_node5_base;
    coord3d coord_node6_pos;
    coord3d coord_node6_base;
    coord3d coord_node7_pos;
    coord3d coord_node7_base;
    coord3d coord_node9_pos;
    coord3d coord_node9_base;
    coord3d coord_node8_pos;
    coord3d coord_node8_base;
    coord3d coord_node10_pos;
    coord3d coord_node10_base;
    coord3d coord_node3_pos;
    Mtxf stickMtx;
    coord3d coord_node3_base;
    coord3d coord_node1_pos;
    coord3d coord_node1_base;
    coord3d coord_node12_pos;
    coord3d coord_node12_base;

    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};

    gunSetWatchMenuWeapon(GUNRIGHT, 0x55);

    if (!(gunHandModelIsEmpty(GUNRIGHT) && gunGetHandModelItem(GUNRIGHT)))
    {
        return gdl;
    }

    objheader = g_CurrentPlayer->handModelHeader;
    matrices = dynAllocate(objheader->numMatrices * (sizeof(Mtxf)));
    modelCalculateRwDataLen(objheader);


    modelInit(&modelstack, objheader, rwdata);
    modelstack.render_pos = (RenderPosView*) matrices;
    mtx4Copy(basemtx, &matrices[0]);
    
    for (i = 1; i < 13; i++)
    {
        position = &objheader->Switches[i]->Data->Group.Origin;

        // Update joy stick position and rotation.
        if (i == 2)
        {
            angle = ((((-(f32) joyGetStickX(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
            mtx4LoadZRotation(angle, &nodeMtx);
            angle = ((((-(f32) joyGetStickY(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
            mtx4LoadXRotation(angle, &rotMtx);
            mtx4MultInPlace(&rotMtx, &nodeMtx);
            mtx4SetTranslation(position, &nodeMtx);
        }
        else
        {
            mtx4LoadTranslation(position, &nodeMtx);
        }

        mtx4Mult(basemtx, &nodeMtx, &matrices[i]);
    }

    modelUpdateNodeRelations(&modelstack);
    renderdata.gdl = gdl;

    if (envcolour >= 0xff)
    {
        renderdata.PropType = PROP_TYPE_OBJ;
    }
    else
    {
        renderdata.PropType = PROP_TYPE_PLAYER;
        renderdata.envcolour.word = envcolour;
        renderdata.fogcolour.word = 0xFFFFFF00;
    }

    renderdata.zbufferenabled = TRUE;
    modelRender(&renderdata, &modelstack);
    gdl = renderdata.gdl;
    mtxPushUnitWorldScale();

    for (i = 0; i < objheader->numMatrices; i++)
    {
        mtx4Copy(&modelstack.render_pos[i].pos, &nodeMtx);
        mtxF2L(&nodeMtx, &modelstack.render_pos[i].pos);
    }

    mtxPopWorldScale();

    if (animatebuttons)
    {
        node = objheader->Switches[13];

        if (node)
        {
            modelGetNodeRwData(&modelstack, node)->Switch.visible = 0;
        }

        matrices = dynAllocate(objheader->numMatrices * (sizeof(Mtxf)));
        modelstack.render_pos = (RenderPosView*) matrices;

        // Update the positions and/or rotations of the free floating buttons on the sides of the screen.
        for (i = 0; i < 13; i++)
        {
            mtx4LoadLookAt(&viewMtx, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
            mtx4LoadLookAt(&buttonMtx, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
            mtx4LoadIdentity(&tiltMtx);
            mtx4LoadIdentity(&identityMtx);
            
            // Joy stick
            if (i == 2)
            {
                coord_node2 = buttonpositions->joystick;
                mtx4LoadTranslation(&coord_node2, &transMtx);
                angle = ((((-(f32) joyGetStickX(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
                mtx4LoadZRotation(angle, &nodeMtx);
                angle = ((((-(f32) joyGetStickY(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
                mtx4LoadXRotation(angle, &rotMtx);
                mtx4MultInPlace(&rotMtx, &nodeMtx);
                mtx4MultInPlace(&transMtx, &nodeMtx);
                mtx4Mult(&viewMtx, &nodeMtx, &tmpMtx);
                mtx4Copy(&tmpMtx, &matrices[i]);
            }
            // R
            else if (i == 11)
            {
                coord_node11_base = ((coord3d *)watchControllerButtonBases)[5];
                coord_node11_pos = buttonpositions->r;

                if (joyGetButtons(*contpadnum, R_TRIG))
                {
                    mtx4LoadYRotation(-0.17453294f, &tiltMtx);
                }

                mtx4LoadXRotation(1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node11_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node11_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Up
            else if (i == 4)
            {
                coord_node4_base = ((coord3d *)watchControllerButtonBases)[6];
                coord_node4_pos = buttonpositions->cUp;

                if (joyGetButtons(*contpadnum, U_CBUTTONS))
                {
                    coord_node4_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node4_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node4_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Down
            else if (i == 5)
            {
                coord_node5_base = ((coord3d *)watchControllerButtonBases)[7];
                coord_node5_pos = buttonpositions->cDown;

                if (joyGetButtons(*contpadnum, D_CBUTTONS))
                {
                    coord_node5_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node5_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node5_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Left
            else if (i == 6)
            {
                coord_node6_base = ((coord3d *)watchControllerButtonBases)[8];
                coord_node6_pos = buttonpositions->cLeft;

                if (joyGetButtons(*contpadnum, L_CBUTTONS))
                {
                    coord_node6_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node6_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node6_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Right
            else if (i == 7)
            {
                coord_node7_base = ((coord3d *)watchControllerButtonBases)[9];
                coord_node7_pos = buttonpositions->cRight;

                if (joyGetButtons(*contpadnum, R_CBUTTONS))
                {
                    coord_node7_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node7_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node7_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // B
            else if (i == 9)
            {
                coord_node9_base = ((coord3d *)watchControllerButtonBases)[10];
                coord_node9_pos = buttonpositions->b;

                if (joyGetButtons(*contpadnum, B_BUTTON))
                {
                    coord_node9_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976, &pressMtx);
                watchControllerButtonMtx(&coord_node9_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node9_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // A
            else if (i == 8)
            {
                coord_node8_base = ((coord3d *)watchControllerButtonBases)[11];
                coord_node8_pos = buttonpositions->a;

                if (joyGetButtons(*contpadnum, A_BUTTON))
                {
                    coord_node8_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node8_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node8_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // L
            else if (i == 10)
            {
                coord_node10_base = ((coord3d *)watchControllerButtonBases)[12];
                coord_node10_pos = buttonpositions->l;

                if (joyGetButtons(*contpadnum, L_TRIG))
                {
                    mtx4LoadYRotation(0.17453294f, &tiltMtx);
                }

                mtx4LoadXRotation(1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node10_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node10_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // D-pad
            else if (i == 3)
            {
                coord_node3_base = ((coord3d *)watchControllerButtonBases)[13];
                coord_node3_pos = buttonpositions->dPad;
                mtx4LoadIdentity(&stickMtx);

                if (joyGetButtons(*contpadnum, U_JPAD))
                {
                    mtx4LoadXRotation(-0.17453294f, &tiltMtx);
                }
                else if (joyGetButtons(*contpadnum, D_JPAD))
                {
                    mtx4LoadXRotation(0.17453294f, &tiltMtx);
                }

                if (joyGetButtons(*contpadnum, L_JPAD))
                {
                    mtx4LoadZRotation(0.17453294f, &stickMtx);
                }
                else if (joyGetButtons(*contpadnum, R_JPAD))
                {
                    mtx4LoadZRotation(-0.17453294f, &stickMtx);
                }

                mtx4MultInPlace(&stickMtx, &tiltMtx);
                mtx4LoadXRotation(-0.89759791f, &pressMtx);
                watchControllerButtonMtx(&coord_node3_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node3_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // Start
            else if (i == 1)
            {
                coord_node1_base = ((coord3d *)watchControllerButtonBases)[14];
                coord_node1_pos = buttonpositions->start;

                if (joyGetButtons(*contpadnum, START_BUTTON))
                {
                    coord_node1_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node1_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node1_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // Z
            else if (i == 12)
            {
                coord_node12_base = ((coord3d *)watchControllerButtonBases)[15];
                coord_node12_pos = buttonpositions->z;

                if (joyGetButtons(*contpadnum, Z_TRIG))
                {
                    mtx4LoadXRotation(-0.17453294f, &tiltMtx);
                }

                mtx4LoadZRotation(M_PI_F, &pressMtx);
                watchControllerButtonMtx(&coord_node12_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node12_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            else
            {
                mtx4Copy(basemtx, &matrices[i]);
            }
        }

        modelUpdateNodeRelations(&modelstack);
        renderdata.gdl = gdl;
        modelRender(&renderdata, &modelstack);
        gdl = renderdata.gdl;
        mtxPushUnitWorldScale();

        for (i = 0; i < objheader->numMatrices; i++)
        {
            mtx4Copy(&modelstack.render_pos[i].pos, &nodeMtx);
            mtxF2L(&nodeMtx, &modelstack.render_pos[i].pos);
        }

        mtxPopWorldScale();
    }

    return gdl;
}


Gfx *watchRenderControllerOpaque(Gfx *gdl, Mtxf *basemtx, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum)
{
    return watchRenderController(gdl, basemtx, 0xff, animatebuttons, buttonpositions, contpadnum);
}


ALSoundState **gunGetFreeSfxState(void)
{
    s32 i;

    for (i = 0; i < NUM_IMPACT_SFX_STATES; i++) 
    {
        if (g_ImpactSfxStates[i] == NULL) 
        {
            return &g_ImpactSfxStates[i];
        }
    }
    
    return NULL;
}


void shotPlayHitPropSound(enum ITEM_IDS item, PropRecord* prop, s32 texture_index)
{
    s32 volume;
    u32 rnd1;
    u32 rnd2;
    ALSoundState **sound_state;
    u32 sfx_index;

    volume = sndVolumeForPosDefault(&prop->pos);

    rnd1 = randomGetNext();
    rnd2 = randomGetNext();



    if ((item == ITEM_REMOTEMINE)
        || (item == ITEM_PROXIMITYMINE)
        || (item == ITEM_TIMEDMINE)
        || (item == ITEM_BOMBCASE)
        || (item == ITEM_BUG)
        || (item == ITEM_MICROCAMERA)
        || (item == ITEM_PLASTIQUE)
        || (item == ITEM_WATCHLASER)
        || (item == ITEM_WATCHMAGNETATTRACT))
    {
        return;
    }

    if (g_ClockTimer <= 0) { return; }

    sound_state = gunGetFreeSfxState();
    if (sound_state != NULL)
    {
        if ((prop->type != PROP_TYPE_CHR) && (prop->type != PROP_TYPE_VIEWER))
        {
            if (item == ITEM_LASER)
            {
                sndPlaySfx(g_musicSfxBufferPtr, RICO_LASER1_SFX, sound_state);
            }
            else
            {
                sndPlaySfx(g_musicSfxBufferPtr, ricochet_sounds_small[rnd1 % 20], sound_state);
            }

            if (*sound_state != NULL)
            {
                sndCreatePostEvent(*sound_state, 8, volume);
            }
        }
        else
        {
            if (item == ITEM_KNIFE)
            {
                sndPlaySfx(g_musicSfxBufferPtr, HIT_BULLET_SNOW_SFX, sound_state);
            }
            else if (item == ITEM_FIST)
            {
                sndPlaySfx(g_musicSfxBufferPtr, punch_sounds[rnd1 % 3], sound_state);
            }
            else
            {
                sndPlaySfx(g_musicSfxBufferPtr, bullet_flesh_sounds[rnd1 % 2], sound_state);
            }

            if (*sound_state != NULL) {
                sndCreatePostEvent(*sound_state, 8, volume);
            }
        }
    }

    sound_state = gunGetFreeSfxState();
    if ((sound_state != NULL) && (texture_index >= 0))
    {
        const SurfaceHit *surface = &g_SurfaceHits[g_Textures[texture_index].hitSound];

        if (surface->numSfx > 0)
        {
            sfx_index = rnd2 % surface->numSfx;
            sndPlaySfx(g_musicSfxBufferPtr, surface->sfx[sfx_index], sound_state);
        }

        if (*sound_state != NULL)
        {
            chrobjSndCreatePostEventDefault(*sound_state, &prop->pos);
        }
    }
}


void shotPlayGlassSound(coord3d* pos)
{
    ALSoundState **sound;

    if (g_ClockTimer <= 0) { return; }

    sound = gunGetFreeSfxState();

    if (sound != NULL)
    {
        sndPlaySfx(g_musicSfxBufferPtr, HIT_BULLET_GLASS_SFX, sound);

        if (*sound != NULL)
        {
            chrobjSndCreatePostEventDefault(*sound, pos);
        }
    }
}


void shotPlayHitSound(enum ITEM_IDS item, coord3d* pos, s32 texture_index)
{
    ALSoundState **sound_state;
    u32 rnd1;
    u32 rnd2;
    u32 sfx_index;
    const SurfaceHit *surface;

    rnd1 = randomGetNext();
    rnd2 = randomGetNext();


    if (g_ClockTimer <= 0) { return; }

    sound_state = gunGetFreeSfxState();
    if (sound_state != NULL)
    {
        if (item != ITEM_WATCHLASER)
        {
            if (item == ITEM_LASER)
            {
                sndPlaySfx(g_musicSfxBufferPtr, laser_ricochet_sounds[rnd1 % 2], sound_state);
            }
            else
            {
                sndPlaySfx(g_musicSfxBufferPtr, ricochet_sounds_large[rnd1 % 36], sound_state);
            }
        }

        if (*sound_state != NULL)
        {
            chrobjSndCreatePostEventDefault(*sound_state, pos);
        }
    }

    sound_state = gunGetFreeSfxState();
    if ((sound_state != NULL) && (texture_index >= 0))
    {
        surface = &g_SurfaceHits[g_Textures[texture_index].hitSound];
        if (surface->numSfx > 0)
        {
            sfx_index = rnd2 % surface->numSfx;
            sndPlaySfx(g_musicSfxBufferPtr, surface->sfx[sfx_index], sound_state);

            if (*sound_state != NULL)
            {
                chrobjSndCreatePostEventDefault(*sound_state, pos);
            }
        }
    }
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


f32 itemGetWatchPosX(ITEM_IDS item)
{
  return gitem_structs[item].watch_pos_x;
}


f32 itemGetWatchPosY(ITEM_IDS item)
{
  return gitem_structs[item].watch_pos_y;
}


f32 itemGetWatchPosZ(ITEM_IDS item)
{
  return gitem_structs[item].watch_pos_z;
}


f32 itemGetWatchRotX(ITEM_IDS item)

{
  return gitem_structs[item].x_rotation;
}


f32 itemGetWatchRotY(ITEM_IDS item)
{
  return gitem_structs[item].y_rotation;
}


f32 itemGetWatchFov(void) {
  return 45.0f;
}


char *itemGetWatchTitle1(ITEM_IDS item)
{
  return langGet(gitem_structs[item].upper_watch_text);
}


char *itemGetWatchTitle2(ITEM_IDS item)
{
    return langGet(gitem_structs[item].lower_watch_text);
}


char *itemGetWatchText(ITEM_IDS item)
{
    return langGet(gitem_structs[item].watch_equipment_text);
}


char *itemGetChoiceText(ITEM_IDS item)
{
    return langGet(gitem_structs[item].weapon_of_choice_text);
}


f32 itemGetEquipFov(void)
{
	return 45.0f;
}


f32 itemGetEquipPosX(ITEM_IDS item)
{
  return gitem_structs[item].equip_watch_x;
}


f32 itemGetEquipPosY(ITEM_IDS item)
{
  return gitem_structs[item].equip_watch_y;
}


f32 itemGetEquipPosZ(ITEM_IDS item)
{
  return gitem_structs[item].equip_watch_z;
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
 * US address 7F067420.
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


extern const f32 g_GunScreenAspectRatio;


#if !defined(VERSION_US)
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
#endif


 
extern const f32 g_CasingSwitchScale;
extern const f32 g_PistolCasingHorizontalSpeed;
extern const f32 g_PistolCasingRotationScaleX;
extern const f32 g_PistolCasingRotationOffsetX;
extern const f32 g_PistolCasingRotationScaleY;
extern const f32 g_PistolCasingRotationOffsetY;
extern const f32 g_PistolCasingRotationScaleZ;
extern const f32 g_PistolCasingRotationOffsetZ;
extern const f32 g_PistolCasingRandomDivisor;
extern const f32 g_PistolCasingGravity;
extern const f32 g_RifleCasingHorizontalSpeed;
extern const f32 g_RifleCasingVerticalSpeed;
extern const f32 g_RifleCasingRotationScaleX;
extern const f32 g_RifleCasingRotationOffsetX;
extern const f32 g_RifleCasingRotationScaleY;
extern const f32 g_RifleCasingRotationOffsetY;
extern const f32 g_RifleCasingRotationScaleZ;
extern const f32 g_RifleCasingRotationOffsetZ;
extern const f32 g_RifleCasingRandomDivisor;
extern const f32 g_RifleCasingGravity;
 
 
/**
 * Ejects a spent cartridge casing from the gun in the given hand.
 */
#if defined(VERSION_EU)
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
#if VERSION_EU
#endif
 
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
 
        // Keep the 0 + 1 for matching.
        if (g_ClockTimer >= (0 + 1))
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
#endif


/* ===== merged from gun2.c ===== */

#if VERSION_EU
extern const f32 g_GunSightAspectRatio;
#endif
extern ALSoundState *g_CasingSfxState;
extern ModelRenderData g_DefaultCasingModelRenderData;

#define AMMO_RELATED_MAX 30
extern AmmoStats ammo_related[AMMO_RELATED_MAX];
extern const char g_GunHudIntegerFormat[];
extern const char aSD[];
extern const char g_GunDeathCountFormat[];
extern const char aSD_0[];


#if defined(VERSION_US)
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
#endif


#if VERSION_EU
#else
#endif
 
 
 
 
/**
 * Ejects a spent cartridge casing from the gun in the given hand.
 */
#if !defined(VERSION_EU)
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
    s32 limit;
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
#if VERSION_EU
    s32 pad[2];
#endif
 
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
 
        // Keep the 0 + 1 for matching.
        if (g_ClockTimer >= (0 + 1))
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
#endif


extern const f32 g_CasingGravity;
extern const f32 g_CasingModelScale;
extern const f32 g_CasingMinMatrixTranslation;
extern const f32 g_CasingMaxMatrixTranslation;

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


void gunSetGunAmmoVisible(s32 reason, bool enable) {

	if (enable)
    {
		g_CurrentPlayer->gunammooff &= ~reason;
		return;
	}

	g_CurrentPlayer->gunammooff |= reason;
}


void ammoGive(s32 ammo_type, s32 ammo_amount) {
    enum ITEM_IDS weapon_id;
    s32 max_ammo;

    weapon_id = gunGetWeapon(GUNRIGHT);
    if ((weaponGetAmmoType(weapon_id) == ammo_type) && (weaponHasFlag(weapon_id, WEAPONSTATBITFLAG_AMMO_CLIP_LIMIT) != 0))
    {
        g_CurrentPlayer->hands[0].weapon_ammo_in_magazine += ammo_amount;
        if (weaponGetStats(weapon_id)->MagSize < g_CurrentPlayer->hands[0].weapon_ammo_in_magazine)
        {
            g_CurrentPlayer->hands[0].weapon_ammo_in_magazine = (s32) weaponGetStats(weapon_id)->MagSize;
        }
        g_CurrentPlayer->ammoheldarr[ammo_type] = 0;
        return;
    }

    max_ammo = ammo_related[ammo_type].MaxAmmo;
    if (max_ammo < ammo_amount)
    {
        g_CurrentPlayer->ammoheldarr[ammo_type] = max_ammo;
        return;
    }

    g_CurrentPlayer->ammoheldarr[ammo_type] = ammo_amount;
}


s32 ammoGetHeld(AMMOTYPE ammotype) {
    return g_CurrentPlayer->ammoheldarr[ammotype];
}

s32 ammoGetTotal(AMMOTYPE ammotype) {

    s32 total_ammo = ammoGetHeld(ammotype);

    if (weaponGetAmmoType(gunGetWeapon(GUNRIGHT)) == ammotype) {
        total_ammo += gunGetMagazineAmmo(GUNRIGHT);
    }

    if (weaponGetAmmoType(gunGetWeapon(GUNLEFT)) == ammotype) {
        total_ammo += gunGetMagazineAmmo(GUNLEFT);
    }

    return total_ammo;
}


s32 ammoGetMax(s32 ammotype)
{
    return ammo_related[ammotype].MaxAmmo;
}


void ammoGiveMaxAll(void)
{
    s32 ammo_type;

    for (ammo_type = 0; ammo_type < AMMO_RELATED_MAX; ammo_type++)
    {
        ammoGive(ammo_type, ammo_related[ammo_type].MaxAmmo);
    }
}


s32 gunGetMagazineAmmo(GUNHAND hand) {
    return g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine;
}


s32 gunGetHeldAmmo(enum GUNHAND hand)
{
    s32 weapon_id;
    s32 ammo_count;

    weapon_id = gunGetWeapon(hand);
    ammo_count = weaponGetHeldAmmo(weapon_id);

    if ((weapon_id == ITEM_SHOTGUN) || (weapon_id == ITEM_AUTOSHOT))
    {
        s32 other_weapon_id;
        other_weapon_id = gunGetWeapon(1 - hand);

        if ((other_weapon_id == ITEM_SHOTGUN) || (other_weapon_id == ITEM_AUTOSHOT))
        {
            return ammo_count - g_CurrentPlayer->hands[1 - hand].numvisibleshells;
        }

        /* I don't know why there's an extra return here, but it's needed to match */
        return ammo_count;
    }

    return ammo_count;
}


s32 weaponGetAmmoType(ITEM_IDS weapon) {
    return weaponGetStats(weapon)->AmmoType;
}

s32 weaponGetHeldAmmo(ITEM_IDS weapon) {
  WeaponStats *weaponstats = weaponGetStats(weapon);
  return g_CurrentPlayer->ammoheldarr[weaponstats->AmmoType];
}

void weaponGiveAmmo(ITEM_IDS weapon, s32 ammo) {
    ammoGive(weaponGetStats(weapon)->AmmoType, ammo);
}

s32 weaponGetMaxAmmo(enum ITEM_IDS weapon)
{
    return ammo_related[weaponGetStats(weapon)->AmmoType].MaxAmmo;
}


Gfx *hudDrawImage(Gfx *gdl, struct sImageTableEntry *tconfig, f32 x, f32 y, f32 baseY, s32 flipX, f32 offsetY, s32 translucent, s32 red, s32 green, s32 blue, s32 alpha)
{
    f32 xy[2];
    f32 halfed[2];
 
    gDPSetColorDither(gdl++, G_CD_DISABLE);
    gDPSetTexturePersp(gdl++, G_TP_NONE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetTextureFilter(gdl++, G_TF_POINT);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    xy[0] = ((u32)tconfig->width * 0.5f) - (f32)(tconfig->width / 2);

    if (flipX != 0) 
    { 
        xy[0] = -xy[0]; 
    }

    xy[0] = xy[0] + x;
 
    if (0.0f <= y)
    {
        xy[1] = y - (((f32) ((u32) tconfig->height)) * 0.5f);
    }
    else
    {
        xy[1] = -((((f32)((u32)tconfig->height)) * 0.5f) - ((f32)(tconfig->height / 2)));
        xy[1] = baseY + xy[1];
        xy[1] = xy[1] + offsetY;
    }
 
    halfed[0] = ((f32) ((u32) tconfig->width)) * 0.5f;
    halfed[1] = ((f32) ((u32) tconfig->height)) * 0.5f;
 
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
    gDPFillRectangle(gdl++, VI_SX(((s32)(xy[0] - halfed[0])) - 1), ((s32)(xy[1] - halfed[1])) - 1, VI_SX(((s32)(xy[0] + halfed[0])) + 1), ((s32)(xy[1] + halfed[1])) + 1);
 
    texSelect(&gdl, tconfig, translucent ? TEXBLEND_TRANSLUCENT : TEXBLEND_OPAQUE, TEXZBUF_OFF, 0);
    texRectDrawImage(&gdl, xy, halfed, tconfig->width, tconfig->height, 0, 0, 1, red, green, blue, alpha, 0 < tconfig->level, 0);
 
    gDPPipeSync(gdl++);
    gDPSetColorDither(gdl++, G_CD_BAYER);
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    return gdl;
}


/**
 * Address: TODO
 * WARNING: This function is missing a "return". This will cause bugs on other compilers.
 */
Gfx *hudDrawImageDefault(Gfx *gdl, sImageTableEntry *tconfig, f32 x, f32 y, f32 baseY, s32 flipX, f32 offsetY, s32 translucent)
{
    return hudDrawImage(gdl, tconfig, x, y, baseY, flipX, offsetY, translucent, 0xff, 0xff, 0xff, 0xff);
}


/**
 * Address: TODO
 */
Gfx *gunDrawHudString(Gfx *gdl, char *text, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    s32 x1;
    s32 y1;
    s32 x2;
    s32 y2;
    s32 textheight;
    s32 textwidth;

    x1 = 0;
    y1 = 0;
    x2 = 0;
    y2 = 0;
    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);

    if (halign == HUDHALIGN_LEFT) { // left
		x2 = x + textwidth;
		x1 = x;
	} else if (halign == HUDHALIGN_RIGHT) { // right
		x1 = x - textwidth;
		x2 = x;
	} else if (halign == HUDHALIGN_MIDDLE) { // middle
		x2 = x + textwidth / 2;
		x1 = x2 - textwidth;
	}

    if (valign == HUDVALIGN_TOP) { // top
		y2 = y + textheight;
		y1 = y;
	} else if (valign == HUDVALIGN_BOTTOM) { // bottom
		y1 = y - textheight;
		y2 = y;
	} else if (valign == HUDVALIGN_MIDDLE) { // middle
		y2 = y + textheight / 2;
		y1 = y2 - textheight;
	}

    gdl = textRenderBlackBox(gdl, &x1, &y1, &x2, &y2);

    if (outline) {
        gdl = textRenderOutlined(gdl, &x1, &y1, text, g_FontBankGothicChars, g_FontBankGothic, -1, 0x646464FF, (s32) viGetX(), viGetY(), 0, 0);
    } else {
        gdl = textRender(gdl, &x1, &y1, text, g_FontBankGothicChars, g_FontBankGothic, 0xFF00B0, (s32) viGetX(), viGetY(), 0, 0);
    }

    return gdl;
}


/**
 * Address: TODO
 */
Gfx *gunDrawHudInteger(Gfx *gdl, s32 value, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    char buffer[12];
    sprintf(buffer, g_GunHudIntegerFormat, value);
    return gunDrawHudString(gdl, buffer, x, halign, y, valign, outline);
}


/**
 * Draw magazine ammo number, ammo type icon, and total ammo number at the bottom right of the viewport.
 * Render an additional ammo counter at the bottom left of the viewport when dual wielding.
 */
Gfx *hudRenderAmmo(Gfx *gdl)
{
    ITEM_IDS weapon_left;
    ITEM_IDS weapon_right;
    s32 ammotype;
    s32 leftx;
    s32 rightx;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset_r;
    s32 textwidth_r;
    u32 imageoffset_l;
    s32 textwidth_l;

    if (g_CurrentPlayer->gunammooff == 0)
    {
        if (g_CurrentPlayer->mpmenuon == 0)
        {
            weapon_left = gunGetWeapon(GUNLEFT);
            weapon_right = gunGetWeapon(GUNRIGHT);

            if (!viewIsHalfWidth())
            {
                leftx = 59;
                rightx = 59;
            }
            else if (coopPlayerPerson(playerGetCurrentNum()) & 1)
            {
                leftx = 43;
                rightx = 127;
            }
            else
            {
                leftx = 59;
                rightx = 109;
            }

            if (weapon_right != ITEM_UNARMED)
            {
                ammotype = weaponGetAmmoType(weapon_right);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_r = ammo_related[ammotype].IconImage;
                    textwidth_r = 5;

                    if (imageoffset_r != 0)
                    {
                        imageoffset_r += g_ImageBankOffset;
                        gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset_r, (camGetScreenLeft() + camGetScreenWidth()) - (f32)rightx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 0,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 0,
#endif
                            ammo_related[ammotype].IconYOffset, 1);
                        textwidth_r = ((u8 *)imageoffset_r)[4];
                    }

                    gdl = textSetupDraw(gdl);

                    if (weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) - (textwidth_r / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) + ((textwidth_r + 1) / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = textEndDraw(gdl);
                }
            }

            if (weapon_left != ITEM_UNARMED)
            {
                ammotype = weaponGetAmmoType(weapon_left);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_l = ammo_related[ammotype].IconImage;
                    textwidth_l = 5;

                    if (imageoffset_l != 0)
                    {
                        imageoffset_l += g_ImageBankOffset;
                        gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset_l, camGetScreenLeft() + (f32)leftx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 1,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 1,
#endif
                            ammo_related[ammotype].IconYOffset, 1);
                        textwidth_l = ((u8 *)imageoffset_l)[4];
                    }

                    gdl = textSetupDraw(gdl);

                    if (weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, ((viGetViewLeft() + leftx) + (textwidth_l / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, ((viGetViewLeft() + leftx) - ((textwidth_l + 1) / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = textEndDraw(gdl);
                }
            }
        }
    }

    return gdl;
}


Gfx *gunDrawWatchAmmoDisplay(Gfx *gdl)
{
    ITEM_IDS offhanditem;
    ITEM_IDS item;
    s32 ammotype;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset;
    s32 textwidth;

    offhanditem = gunGetWeapon(1);
    item = gunGetWeapon(0);

    if (item != ITEM_UNARMED)
    {
        ammotype = weaponGetAmmoType(item);

        if (ammotype != 0
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
            && !weaponHasFlag(item, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
        {
            imageoffset = ammo_related[ammotype].IconImage;
            textwidth = 5;

            weaponGetStats(item);

            if (imageoffset != 0)
            {
                imageoffset += g_ImageBankOffset;

                // Draw the ammo icon
#if defined(VERSION_EU)
                gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset, 200.0f, 208.0f, (viGetViewTop() + viGetViewHeight()) - 30, 0, ammo_related[ammotype].IconYOffset, 1);
#else
                gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset, 200.0f, 180.0f, (viGetViewTop() + viGetViewHeight()) - 20, 0, ammo_related[ammotype].IconYOffset, 1);
#endif

                textwidth = ((u8 *)imageoffset)[4];
            }

            gdl = textSetupDraw(gdl);

            if (weaponHasFlag(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                magammo = 0;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;

                if (offhanditem == item)
                {
                    reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                }
            }
            else
            {
                magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
            }

            if (!weaponHasFlag(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the magazine ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 177, 2, 0);
#endif
            }

            if (reserveammo > 0 || weaponHasFlag(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the reserve ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 177, 2, 0);
#endif
            }

            gdl = textEndDraw(gdl);
        }
    }

    return gdl;
}


void gunSetSightVisible(s32 reason, bool visible)
{
    if (visible)
    {
        g_CurrentPlayer->gunsightmode &= ~reason;
        return;
    }

    g_CurrentPlayer->gunsightmode |= reason;
}


void gunDrawSight(Gfx **gdl) {

    Gfx *dl;
    f32 xypos[2];
    f32 halfedxy[2];

    if ((g_CurrentPlayer->gunsightmode == 0) && (g_CurrentPlayer->mpmenuon == FALSE)) {
        dl = *gdl;
        texSelect(&dl, g_ImageCrosshair, TEXBLEND_CLOUD, TEXZBUF_OFF, 0);

        xypos[0] = g_CurrentPlayer->crosshair_angle.f[0];
        xypos[1] = g_CurrentPlayer->crosshair_angle.f[1];
        halfedxy[0] = 16.0f;
        halfedxy[1] = 16.0f;

        if (optionsGetScreenRatio() == SCREEN_RATIO_16_9) {
            halfedxy[0] = halfedxy[0] * 0.75f;
        }
#ifdef VERSION_EU
        halfedxy[1] = halfedxy[1] * g_GunSightAspectRatio;
#endif
        texRectDrawImage(&dl, xypos, halfedxy, 0x20, 0x20, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0x6E, (g_ImageCrosshair->level > 0), 0);
        *gdl = dl;
    }
}


void statsAddHit(ITEM_IDS item, SHOT_REGISTER shot_register) {

    if (weaponHasFlag(item, WEAPONSTATBITFLAG_PLAYER_STAT_HIT)) {
        g_playerPerm->shot_count[shot_register] = g_playerPerm->shot_count[shot_register]+1;
    }
}


s32 statsGetHits(SHOT_REGISTER shot_register)
{
  return g_playerPerm->shot_count[shot_register];
}


void statsAddCivilianKill(void)
{
    g_playerPerm->killed_civilians++;
}


s32 statsGetCivilianKills(void)
{
    return g_playerPerm->killed_civilians;
}


void statsAddKill(void)
{
    char buffer[256];
    s32 time_since_kill;
    s32 recent_kill_count;
    s32 mission_time;

    g_playerPerm->kill_count += 1;
    g_CurrentPlayer->kills_this_life += 1;

    if (!isMpGame()) { return; }

    mission_time = lvGetMissionTime60();
    sprintf(buffer, aSD, langGet(getStringID(LGUN, GUN_STR_DA_KILLCOUNT)), g_playerPerm->kill_count); // "kill count"

#if defined(VERSION_US)
    hudmsgBottomShow(buffer);
#elif defined(VERSION_EU)
    hudmsgBottomShowText(buffer);
#endif

    if (g_playerPerm->kill_count >= 2)
    {
        time_since_kill = mission_time - g_CurrentPlayer->last_kill_time[0];
        if (g_playerPerm->max_time_between_kills < time_since_kill)
        {
            g_playerPerm->max_time_between_kills = time_since_kill;
        }

        if (time_since_kill < g_playerPerm->min_time_between_kills)
        {
            g_playerPerm->min_time_between_kills = time_since_kill;
        }
    }

    recent_kill_count = 1;
    g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
    g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
    g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
    g_CurrentPlayer->last_kill_time[0] = mission_time;

    // I tried to turn this into a loop but it didn't match
    if (g_CurrentPlayer->last_kill_time[1] != -1 && (g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78)
    {
        recent_kill_count++;
        if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78))
        {
            recent_kill_count++;
            if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78))
            {
                recent_kill_count++;
            }
        }
    }

    if (g_playerPerm->most_killed_one_time < recent_kill_count)
    {
        g_playerPerm->most_killed_one_time = recent_kill_count;
    }
}


s32 statsGetKills(void) {
    return g_playerPerm->kill_count;
}

void statsAddGoldenGunKill(void){
    g_playerPerm->killed_gg_owner_count++;
}


void statsAddDeath(void)
{
	char buffer[256];
    g_CurrentPlayer->deathcount = (s32) (g_CurrentPlayer->deathcount + 1);
    if (isMpGame())
    {
        if (g_CurrentPlayer->deathcount == 1)
        {
            sprintf(buffer, langGet(getStringID(LGUN, GUN_STR_DB_DIEDONCE_LF))); //died once
        }
        else
        {
            sprintf(buffer, g_GunDeathCountFormat, langGet(getStringID(LGUN, GUN_STR_DC_DIED)), g_CurrentPlayer->deathcount, langGet(getStringID(LGUN, GUN_STR_DD_TIMES))); //died times
        }
#if defined(VERSION_EU)
		hudmsgBottomShowText(buffer);
#else
		hudmsgBottomShow(buffer);
#endif
    }
}


void statsAddSuicide(void) {
    char buffer[256];
    s32 time_diff;
    s32 recent_kill_count;
    s32 currentTime;

    g_CurrentPlayer->num_suicides += 1;
    if (isMpGame()) {

        currentTime = lvGetMissionTime60();

        sprintf(buffer, aSD_0, langGet(getStringID(LGUN, GUN_STR_DE_SUICIDECOUNT)), g_CurrentPlayer->num_suicides); // "suicide count"

#if defined(VERSION_EU)
		hudmsgBottomShowText(buffer);
#else
		hudmsgBottomShow(buffer);
#endif

        if (g_playerPerm->kill_count >= 2) {
            time_diff = currentTime - g_CurrentPlayer->last_kill_time[0];
            if (g_playerPerm->max_time_between_kills < time_diff) {
                g_playerPerm->max_time_between_kills = time_diff;
            }
            if (time_diff < g_playerPerm->min_time_between_kills) {
                g_playerPerm->min_time_between_kills = time_diff;
            }
        }
        recent_kill_count = 1;
        g_CurrentPlayer->last_kill_time[3] = g_CurrentPlayer->last_kill_time[2];
        g_CurrentPlayer->last_kill_time[2] = g_CurrentPlayer->last_kill_time[1];
        g_CurrentPlayer->last_kill_time[1] = g_CurrentPlayer->last_kill_time[0];
        g_CurrentPlayer->last_kill_time[0] = currentTime;

        if ( g_CurrentPlayer->last_kill_time[1] != -1) {

            if ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[1]) < 0x78) {

                recent_kill_count += 1;

                if ((g_CurrentPlayer->last_kill_time[2] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[2]) < 0x78)) {

                    recent_kill_count += 1;

                    if ((g_CurrentPlayer->last_kill_time[3] != -1) && ((g_CurrentPlayer->last_kill_time[0] - g_CurrentPlayer->last_kill_time[3]) < 0x78)) {
                        recent_kill_count += 1;
                    }
                }
            }
        }

        if (g_playerPerm->most_killed_one_time < recent_kill_count) {
            g_playerPerm->most_killed_one_time = recent_kill_count;
        }
    }
}

/*
 * IDO emits scalar const objects to .data. The linker keeps this block directly
 * after gunfire's .rodata so these values retain their original ROM layout.
 */
#if VERSION_EU
const f32 g_GunScreenAspectRatio = 20.0f / 17.0f;
#else
const f32 g_GunScreenAspectRatio = 4.0f / 3.0f;
#endif
const f32 g_CasingSwitchScale = 0.10000001f;
const f32 g_PistolCasingHorizontalSpeed = 0.5333333f;
const f32 g_PistolCasingRotationScaleX = M_TAU_F;
const f32 g_PistolCasingRotationOffsetX = M_PI_F / 8.0f;
const f32 g_PistolCasingRotationScaleY = M_TAU_F;
const f32 g_PistolCasingRotationOffsetY = M_PI_F / 8.0f;
const f32 g_PistolCasingRotationScaleZ = M_TAU_F;
const f32 g_PistolCasingRotationOffsetZ = M_PI_F / 8.0f;
#if VERSION_EU
const f32 g_PistolCasingRandomDivisor = 931050.0f;
#else
const f32 g_PistolCasingRandomDivisor = 775875.0f;
#endif
const f32 g_PistolCasingGravity = 0.2777778f;
const f32 g_RifleCasingHorizontalSpeed = 1.4166666f;
const f32 g_RifleCasingVerticalSpeed = 1.6666666f;
const f32 g_RifleCasingRotationScaleX = M_TAU_F;
const f32 g_RifleCasingRotationOffsetX = M_PI_F / 8.0f;
const f32 g_RifleCasingRotationScaleY = M_TAU_F;
const f32 g_RifleCasingRotationOffsetY = M_PI_F / 8.0f;
const f32 g_RifleCasingRotationScaleZ = M_TAU_F;
const f32 g_RifleCasingRotationOffsetZ = M_PI_F / 8.0f;
#if VERSION_EU
const f32 g_RifleCasingRandomDivisor = 931050.0f;
#else
const f32 g_RifleCasingRandomDivisor = 775875.0f;
#endif
const f32 g_RifleCasingGravity = 0.2777778f;
const f32 g_CasingGravity = 0.2777778f;
const f32 g_CasingModelScale = 0.10000001f;
const f32 g_CasingMinMatrixTranslation = -30000.0f;
const f32 g_CasingMaxMatrixTranslation = 30000.0f;
#if VERSION_EU
const f32 g_GunSightAspectRatio = 25.0f / 21.0f;
#endif


/**
 * Tests if a ray intersects the bounding box of the given room.
 * @return TRUE if the ray intersects, otherwise FALSE.
*/
s32 shotRayHitsRoomBbox(s32 room, coord3d* start, coord3d* dir)
{
    s32 max[3];
    s32 min[3];
    s_room_info* roominfo;

    roominfo = &g_BgRoomInfo[room];

    // Skip check if room has no collision data
    if (roominfo->vtx_batch_bounds != NULL)
    {
        min[0] = roominfo->minbounds.f[0];
        min[1] = roominfo->minbounds.f[1];
        min[2] = roominfo->minbounds.f[2];
        max[0] = roominfo->maxbounds.f[0];
        max[1] = roominfo->maxbounds.f[1];
        max[2] = roominfo->maxbounds.f[2];
        if (bgTestRayIntersectsBbox(start, dir, min, max)) {
            return TRUE;
        }
    }
    return FALSE;
}


/**
 * Refines an existing background bullet hit by checking currently visible rooms
 * that have not already been tested.
 *
 * The function scans the visible room list, marks each tested room in visited,
 * performs a room bbox test first, then tests the room geometry. If no previous hit exists,
 * the first visible room hit is accepted.
 * Otherwise, a hit is accepted only if it lies between from and the current
 * best hit on all three axes, making it closer along the shot ray.
 *
 * @return Returns the room number of the accepted hit, or the bestroom if no
 * closer visible room hit is found.
 */
s32 shotFindBgHitInVisibleRooms(coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *besthit, s32 bestroom)
{
    s32 rooms[100];
    s32 *roomptr;
    s32 *end;
    s32 numrooms;
    struct HitThing hit;
    f32 scale;
    s32 room;

    scale = bgGetLevelScaleInv();

    // Get up to 100 currently visible rooms.
    numrooms = bgCopyVisibleRoomsToList(&rooms[0], 100);

    if (numrooms > 0)
    {
        roomptr = rooms;
        // The bitwise AND is just a matching trick and effectively does nothing.
        end = roomptr + (numrooms & 0xFFFFFFFF);

        do
        {
            // Only check rooms that have not been visited.
            if (visited[*roomptr] == 0)
            {
                visited[*roomptr] = 1;

                if (shotRayHitsRoomBbox(*roomptr, scaledDir, dir))
                {
                    if (bgTestBulletHitBackground(from, to, *roomptr, &hit))
                    {
                        room = *roomptr;
                        hit.hitpos.x *= scale;
                        hit.hitpos.y *= scale;
                        hit.hitpos.z *= scale;

                        /**
                         * The (numrooms * 0) is weird but harmless and needed for matching.
                         */
                        if ((bestroom <= (numrooms * 0))
                                || (((((from->x <= besthit->hitpos.x)
                                            && (from->x <= hit.hitpos.x))
                                            && (hit.hitpos.x < besthit->hitpos.x))
                                        || (((besthit->hitpos.x <= from->x)
                                            && (hit.hitpos.x <= from->x))
                                            && (besthit->hitpos.x < hit.hitpos.x)))
                                    && ((((from->y <= besthit->hitpos.y)
                                            && (from->y <= hit.hitpos.y))
                                            && (hit.hitpos.y < besthit->hitpos.y))
                                        || (((besthit->hitpos.y <= from->y)
                                            && (hit.hitpos.y <= from->y))
                                            && (besthit->hitpos.y < hit.hitpos.y)))
                                    && ((((from->z <= besthit->hitpos.z)
                                            && (from->z <= hit.hitpos.z))
                                            && (hit.hitpos.z < besthit->hitpos.z))
                                        || (((besthit->hitpos.z <= from->z)
                                            && (hit.hitpos.z <= from->z))
                                            && (besthit->hitpos.z < hit.hitpos.z)))))
                        {
                            bestroom = room;
                            *besthit = hit;
                        }
                    }
                }
            }

            roomptr++;
        }
        while (roomptr < end);

    }

    return bestroom;
}


/**
 * Beginning at startroom, walk connected rooms looking for a background
 * bullet hit.
 *
 * Rooms are skipped if already marked in visited, and newly processed rooms are
 * marked visited.
 * @return Return 0 if no hit is found, otherwise the room number of the first room whose bbox and
 * background geometry intersect the bullet ray.
 */
s32 shotFindBgHitInConnectedRooms(s32 startroom, coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *hit)
{
    u8 rooms[256];
    s32 neighbours[100];
    s32 numneighbours;
    s32 i;
    s32 j;
    s32 count;
    s32 curindex;
    s32 room;

    rooms[0] = startroom;
    count = 1;

    for (curindex = 0; curindex < count; curindex++)
    {
        room = rooms[curindex];

        if (visited[room] == 0)
        {
            visited[room] = 1;

            if (shotRayHitsRoomBbox(room, scaledDir, dir))
            {
                if (bgTestBulletHitBackground(from, to, room, hit))
                {
                    return room;
                }
            }
        }

        numneighbours = bgGetConnectedRooms(room, neighbours, 100);

        for (i = 0; i < numneighbours; i++)
        {
            for (j = 0; j < count; j++)
            {
                if (rooms[j] == neighbours[i])
                {
                    break;
                }
            }

            if (j == count)
            {
                rooms[count] = neighbours[i];
                count++;
            }
        }
    }

    return 0;
}


/**
 * Finds the closest bg bullet collision among rooms not already visited by the shot traversal.
 * It first does a cheap bounding box test, then a precise test for rooms whose bounding boxes are intersected.
 * This seems to be a brute force/fallback version of the function above, shotFindBgHitInConnectedRooms.
 * @return 0 if no bg hit in any unvisited room, otherwise the room number containing the closest bg hit.
 */
s32 shotFindBgHit(coord3d *from, coord3d *to, coord3d *dir, coord3d *scaledDir, u8 *visited, struct HitThing *besthit)
{
    f32 dx;
    f32 dy;
    struct HitThing hit;
    f32 scale;
    f32 dist;
    f32 adjusteddist;
    f32 bestdist;
    s32 bestroom;
    f32 tmp;
    s32 room;

    bestdist = M_U32_MAX_VALUE_F;
    bestroom = 0;

    scale = bgGetLevelScaleInv();

    room = 1;

    if (bgGetMaxNumRooms() >= 2)
    {
        do
        {
            if (visited[room] == 0)
            {
                visited[room] = 1;

                if (shotRayHitsRoomBbox(room, scaledDir, dir))
                {
                    if (bgTestBulletHitBackground(from, to, room, &hit))
                    {
                        dx = (hit.hitpos.x * scale) - from->x;
                        dy = ((hit.hitpos.y * scale) - from->y) * 1.0f;
                        dist = (hit.hitpos.z * scale) - from->z;
                        dist = (tmp = ((dx * dx) + (dy * dy)) + (dist * dist));
                        adjusteddist = tmp;

                        if (lightIsLightTexture(hit.texturenum))
                        {
                            adjusteddist = tmp - 4.0f;
                        }

                        if (adjusteddist < bestdist)
                        {
                            besthit->hitpos.x = hit.hitpos.x;
                            besthit->hitpos.y = hit.hitpos.y;
                            besthit->hitpos.z = hit.hitpos.z;

                            bestdist = adjusteddist;
                            bestroom = room;

                            besthit->normal.x = hit.normal.x;
                            besthit->normal.y = hit.normal.y;
                            besthit->normal.z = hit.normal.z;

                            besthit->vtx0 = hit.vtx0;
                            besthit->vtx1 = hit.vtx1;
                            besthit->vtx2 = hit.vtx2;

                            besthit->texturenum = hit.texturenum;
                            besthit->tricmd = hit.tricmd;
                            besthit->triNum = hit.triNum;
                        }
                    }
                }
            }

            room++;
        }
        while (room < bgGetMaxNumRooms());
    }

    return bestroom;
}

/*
* This function has a bunch of issues. It allows the player to shoot through walls
* and even creates sparks in the distance when they shoot at the sky. Some of these
* issues arise because it traces over the stan tiles to find the first room to test.
* This leads to the player being able to shoot through floors if they have a portal
* in front of them (because it's a room boundary which leads to another room) as
* the code tracing over stan tiles only cares about the furthest stan's room. Luckily,
* none of those bugs affect props, so prop hits are always the best candidate.
*/
void shotFireBullet(s32 hand)
{
    f32 maxRange;
    coord3d *playerpos;
    s32 hitbgstan;
    coord3d stanhit;
    StandTile *hittile;
    s32 createSpark;
    s32 gotbghit;
    coord3d visiblehitpos;
    s32 bestroom;
    s32 besttexture = 0;
    HitThing bghit;
    f32 negz = 0.0f;
    coord3d besthitpos;
    s32 pad;
    StandTile *fromtile;
    coord3d dest;
    ShotData shotdata;
    coord3d *finalpos;
    s32 numhits;
    u8 visited[256];
    const SurfaceHit *surface;
    coord3d scaleddir;
    coord3d hitdir;
    f32 distscale;
    PropRecord *playerprop;
    PropRecord *prop;
    PropRecord **pp;
    s32 startroom;
    s32 k;
    s32 i;
    u8 rooms[2];

    hitbgstan = 0;
    hittile = 0;
    gotbghit = 0;
    bestroom = 0;
    playerprop = currentPlayerGetProp();
    fromtile = playerprop->stan;
    numhits = 0;
    gunGetShotDir(&shotdata.viewOrigin, &shotdata.viewDir, hand);
    shotdata.weapon = gunGetWeapon(hand);
    shotdata.maxdist = M_U32_MAX_VALUE_F;

    for (k = 0; k < 10; k++)
    {
        shotdata.hits[k].prop = 0;
        shotdata.hits[k].hitpart = 0;
        shotdata.hits[k].node = 0;
    }

    shotdata.gunpos.x = shotdata.viewOrigin.x;
    shotdata.gunpos.y = shotdata.viewOrigin.y;
    shotdata.gunpos.z = shotdata.viewOrigin.z;

    mtx4TransformVecInPlace(camGetViewToWorldMtxf(), &shotdata.gunpos);

    shotdata.dir.x = shotdata.viewDir.x;
    shotdata.dir.y = shotdata.viewDir.y;
    shotdata.dir.z = shotdata.viewDir.z;

    mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &shotdata.dir);

    dest.x = (shotdata.dir.x * M_U16_MAX_VALUE_F) + shotdata.gunpos.x;
    dest.y = (shotdata.dir.y * M_U16_MAX_VALUE_F) + shotdata.gunpos.y;
    dest.z = (shotdata.dir.z * M_U16_MAX_VALUE_F) + shotdata.gunpos.z;

    if (stanWalkLineNoCallback(&fromtile, playerprop->pos.x, playerprop->pos.z, shotdata.gunpos.x, shotdata.gunpos.z))
    {
        distscale = bgGetLevelScale() * bgGetLevelVisibilityScale();
        playerpos = playerGetPos();



        stanResetHits();

        if (!stanWalkLineNoCallback(&fromtile, shotdata.gunpos.x, shotdata.gunpos.z, dest.x, dest.z))
        {
            chrStanLineDirIntersection(&shotdata.gunpos, &shotdata.dir, &stanhit);
            hitbgstan = 1;
        }
        else
        {
            stanhit.x = dest.x;
            stanhit.y = dest.y;
            stanhit.z = dest.z;
        }

        hitdir.x = stanhit.x - playerpos->x;
        hitdir.y = stanhit.y - playerpos->y;
        hitdir.z = stanhit.z - playerpos->z;
        scaleddir.x = playerpos->x * distscale;
        scaleddir.y = playerpos->y * distscale;
        scaleddir.z = playerpos->z * distscale;
        hittile = fromtile;
        startroom = stanGetTileRoom(fromtile);

        for (i = 0; i < 256; i++)
        {
            visited[i] = 0;
        }

        if (bgTestBulletHitBackground(playerpos, &stanhit, startroom, &bghit))
        {
            bestroom = startroom;
        }

        visited[startroom] = 1;

        if (bestroom <= 0)
        {
            if (g_BgPortals[0].offset_portal != 0)
            {
                bestroom = shotFindBgHitInConnectedRooms(stanGetTileRoom(currentPlayerGetProp()->stan), playerpos, &stanhit, &hitdir, &scaleddir, visited, &bghit);
            }
            else
            {
                bestroom = shotFindBgHit(playerpos, &stanhit, &hitdir, &scaleddir, visited, &bghit);
            }
        }

        if (bestroom > 0)
        {
            distscale = bgGetLevelScaleInv();
            bghit.hitpos.x *= distscale;
            bghit.hitpos.y *= distscale;
            bghit.hitpos.z *= distscale;
        }

        bestroom = shotFindBgHitInVisibleRooms(playerpos, &stanhit, &hitdir, &scaleddir, visited, &bghit, bestroom);

        if (bestroom > 0)
        {
            gotbghit = 1;
            besttexture = bghit.texturenum;
            besthitpos.f[0] = (visiblehitpos.f[0] = bghit.hitpos.f[0]);
            besthitpos.f[1] = (visiblehitpos.f[1] = bghit.hitpos.f[1]);
            besthitpos.f[2] = (visiblehitpos.f[2] = bghit.hitpos.f[2]);
        }
        else
        {
            bestroom = startroom;
            besttexture = -1;
            besthitpos.x = dest.x;
            besthitpos.y = dest.y;
            besthitpos.z = dest.z;
        }

        if (hitbgstan || gotbghit)
        {
            mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &besthitpos);
            negz = -besthitpos.f[2];
            shotdata.maxdist = negz;
        }
    }

    maxRange = 300.0f;

    if ((shotdata.weapon == 23) && (shotdata.maxdist > maxRange))
    {
        shotdata.maxdist = maxRange;
    }

    for (pp = g_LastOnScreenProp; (--pp) >= g_OnScreenPropList;)
    {
        prop = *pp;

        if (prop != 0)
        {
            if ((prop->type == PROP_TYPE_CHR) || (((prop->type == PROP_TYPE_VIEWER) && (prop->chr != 0)) && (playerGetIndexForProp(prop) != playerGetCurrentNum())))
            {
                chrTestHit(prop, &shotdata);
            }
            else if (((prop->type == PROP_TYPE_OBJ) || (prop->type == PROP_TYPE_WEAPON)) || (prop->type == PROP_TYPE_DOOR))
            {
                objTestShot(prop, &shotdata);
            }
        }
    }

    for (k = 0; k < 10; k++)
    {
        if (shotdata.hits[k].prop != 0)
        {
            if ((shotdata.hits[k].prop->type == PROP_TYPE_CHR) || (shotdata.hits[k].prop->type == PROP_TYPE_VIEWER))
            {
                chrHandleBulletHit(&shotdata, &shotdata.hits[k]);
            }
            else if (((shotdata.hits[k].prop->type == PROP_TYPE_OBJ) || (shotdata.hits[k].prop->type == PROP_TYPE_WEAPON)) || (shotdata.hits[k].prop->type == PROP_TYPE_DOOR))
            {
                objHit(&shotdata, &shotdata.hits[k]);
            }

            if (shotdata.hits[k].countsAsPenetration)
            {
                numhits++;

                if (numhits >= weaponGetPenetration(shotdata.weapon))
                {
                    gotbghit = 0;
                    hitbgstan = 0;
                }
            }
        }
    }

    if (gotbghit || hitbgstan)
    {
        finalpos = 0;
        createSpark = 1;

        if ((shotdata.weapon == 23) && (negz > maxRange))
        {
            createSpark = 0;
        }

        if (gotbghit)
        {
            if (bghit.texturenum < 0)
            {
                surface = &g_SurfaceHits[HIT_DEFAULT];
            }
            else
            {
                surface = &g_SurfaceHits[g_Textures[bghit.texturenum].hitTexture];
            }

            if (createSpark)
            {
                if ((surface->numImpacts > 0) && (shotdata.weapon != 23))
                {
                    pad = randomGetNext() % surface->numImpacts;
                    explosionCreateBulletImpact(&visiblehitpos, &bghit.normal, surface->impacts[pad], bestroom, 0, -1, 0);
                }

                if (lightIsLightTexture(bghit.texturenum))
                {
                    lightFixtureBreak(bghit.tricmd, bghit.triNum, bestroom);
                }
            }

            finalpos = &visiblehitpos;
        }
        else if (hitbgstan)
        {
            stanhit.x = (shotdata.dir.x * M_U16_MAX_VALUE_F) + shotdata.gunpos.x;
            stanhit.y = (shotdata.dir.y * M_U16_MAX_VALUE_F) + shotdata.gunpos.y;
            stanhit.z = (shotdata.dir.z * M_U16_MAX_VALUE_F) + shotdata.gunpos.z;
            finalpos = &stanhit;
        }

        if (finalpos != 0)
        {
            if (createSpark)
            {
                shotPlayHitSound(shotdata.weapon, finalpos, besttexture);

                // No impact puff on water (5) or snow (6). A shot that hit no background
                // triangle has no texture (-1) and always gets one.
                if (besttexture < 0
                    || (g_Textures[besttexture].hitTexture != 5 && g_Textures[besttexture].hitTexture != 6))
                {
                    rooms[0] = bestroom;
                    rooms[1] = 255;
                    explosionCreate(0, finalpos, hittile, 1, 0, playerGetCurrentNum(), rooms, 0);
                }
            }

            finalpos->x -= 26.0f * shotdata.dir.x;
            finalpos->y -= 26.0f * shotdata.dir.y;
            finalpos->z -= 26.0f * shotdata.dir.z;

            gunSetTracerTarget(finalpos);

            if (createSpark)
            {
                sparkCreate(finalpos, 1, 26.0f, bestroom);
            }
        }
    }
}


/**
 * Hitscans gather candidate hits along the bullet path. This function records each candidate hit into shotdata
 * and enforces pentration limits and removes hits that should be blocked by closer objects.
 */
void shotAddHit(struct ShotData *shotdata, PropRecord *prop, f32 dist, s32 hitpart, ModelNode *node, struct HitThing *hitthing, s32 room, ModelNode *bboxnode, Model *model, bool countsAsPenetration, s32 blocksFurtherHits)
{
    s32 i;
    s32 furthestindex;
    f32 prevfurthest;
    s32 numPenetratedObjects;
    f32 furthest;

    /**
     * If countsAsPenetration is true, then this hit is on an object that bullets may pass through,
     * and it counts against the weapon's shoot-through object limit.
     */
    if (countsAsPenetration)
    {
        furthest = 0.0f;
        prevfurthest = furthest;
        furthestindex = 0;
        numPenetratedObjects = 0;

        for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
        {
            if (shotdata->hits[i].prop != NULL && shotdata->hits[i].countsAsPenetration)
            {
                numPenetratedObjects++;

                if (furthest < shotdata->hits[i].dist)
                {
                    prevfurthest = furthest;
                    furthest = shotdata->hits[i].dist;
                    furthestindex = i;
                }
            }
        }

        /**
         * The bullet has reached the max number of objects it can penetrate.
         */
        if (numPenetratedObjects >= weaponGetPenetration(shotdata->weapon))
        {
            // Make room for this new hit.
            shotdata->hits[furthestindex].prop = NULL;
            shotdata->maxdist = prevfurthest;

            // Update the shot's useful distance.
            if (prevfurthest < dist)
            {
                shotdata->maxdist = dist;
            }

            // Remove hits that are beyond the penetration limit.
            for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
            {
                if (shotdata->hits[i].prop != NULL && (!shotdata->hits[i].countsAsPenetration) && prevfurthest < shotdata->hits[i].dist)
                {
                    shotdata->hits[i].prop = NULL;
                }
            }
        }
        else
        {
            /**
             * This hit is the final allowed penetrable object.
             */
            if (numPenetratedObjects + 1 == weaponGetPenetration(shotdata->weapon))
            {
                if (dist < shotdata->maxdist)
                {
                    shotdata->maxdist = dist;
                }
            }
        }
    }

    /**
     * If true, this stops the bullets for all weapons except the Cougar Magnum and Silver PP7.
     * Any already recorded hits farther than this one are removed,
     * and the shot's max distance is clamped to this hit distance.
     * Used by bulletproof glass.
    */
    if (blocksFurtherHits)
    {
        if (shotdata->weapon != ITEM_RUGER && shotdata->weapon != ITEM_SILVERWPPK)
        {
            for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
            {
                if (shotdata->hits[i].prop != NULL && dist < shotdata->hits[i].dist)
                {
                    shotdata->hits[i].prop = NULL;
                }
            }

            shotdata->maxdist = dist;
        }
    }

    for (i = 0; i < ARRAYCOUNT(shotdata->hits); i++)
    {
        if (shotdata->hits[i].prop == NULL)
        {
            shotdata->hits[i].dist = dist;
            shotdata->hits[i].prop = prop;
            shotdata->hits[i].hitpart = hitpart;
            shotdata->hits[i].node = node;
            shotdata->hits[i].hit = *hitthing;
            shotdata->hits[i].room = room;
            shotdata->hits[i].bboxNode = bboxnode;
            shotdata->hits[i].model = model;
            shotdata->hits[i].countsAsPenetration = countsAsPenetration;
            break;
        }
    }
}


void shotPunch(s32 hand, s32 item_id)
{
    PropRecord *playerprop;
    f32 ducking;
    s32 hit;
    PropRecord *prop;
    PropRecord **propptr;
    ChrRecord *chr;
    f32 max0;
    f32 min0;
    f32 max1;
    f32 min1;
    f32 max2;
    f32 min2;
    s32 hitpart;
    StandTile *tile;
    coord3d from;
    coord3d vector;
    f32 reach;

    hit = 0;
    playerprop = currentPlayerGetProp();
    ducking = playerGetEyeHeight(g_CurrentPlayer);

    for (propptr = g_LastOnScreenProp - 1; propptr >= g_OnScreenPropList; propptr--)
    {
        prop = *propptr;

        if (prop == NULL)
        {
            continue;
        }

        if (!(prop->zDepth < 500.0f))
        {
            continue;
        }

        if (prop->type != PROP_TYPE_CHR)
        {
            if (prop->type != PROP_TYPE_VIEWER)
            {
                continue;
            }

            if (prop->chr == NULL)
            {
                continue;
            }

            if (playerGetIndexForProp(prop) == playerGetCurrentNum())
            {
                continue;
            }
        }

        reach = 50.0f;
        chr = prop->chr;

        if (gunGetFistModelItem() == ITEM_SNIPERRIFLE)
        {
            reach = 100.0f;
        }

        modelGetAxisExtents(chr->model, &max0, &min0, 0);

        if (!(0.0f <= max0))
        {
            continue;
        }

        if (!(min0 <= 0.0f))
        {
            continue;
        }

        modelGetAxisExtents(chr->model, &max1, &min1, 1);

        if (!(0.0f <= max1))
        {
            continue;
        }

        if (!(min1 <= 0.0f))
        {
            continue;
        }

        modelGetAxisExtents(chr->model, &max2, &min2, 2);

        if (!(min2 <= 0.0f))
        {
            continue;
        }

        if (!((-reach) <= max2))
        {
            continue;
        }

        tile = playerprop->stan;

        if (!stanTestLineUnobstructed(&tile, playerprop->pos.x, playerprop->pos.z, prop->pos.x, prop->pos.z, CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, ducking, ducking, 0.0f, 1.0f))
        {
            continue;
        }

        if (tile != prop->stan) {
            continue;
        }

        hitpart = HIT_CHEST;

        if (currentPlayerGetCrouchPos() == CROUCH_HALF) {
            hitpart = HIT_GENERAL;
        }
        else if (currentPlayerGetCrouchPos() == CROUCH_SQUAT)
        {
            hitpart = HIT_GENERALHALF;
        }


        gunGetShotDir(&from, &vector, hand);
        mtx4RotateVecInPlace(camGetViewToWorldMtxf(), &vector);

        if (chrTakeShot(chr, hitpart, &vector, item_id, 1))
        {
            shotPlayHitPropSound(item_id, prop, -1);
            hit = 1;
        }
    }

    if ((!hit) && (item_id == ITEM_FIST))
    {
        sndPlaySfx(g_musicSfxBufferPtr, PUNCHING_AIR_SFX, 0);
    }
}


void shotFireBullet(s32);

void shotFireHand(s32 hand)
{
    s32 item_id;
    s32 i;

    if (gunGetFiringStatus(hand) != 0)
    {
        item_id = gunGetWeapon(hand);

        if (item_id == ITEM_TRIGGER)
        {
            remoteMinesDetonate();
        }
        else if (item_id == ITEM_GRENADELAUNCH
            || item_id == ITEM_ROCKETLAUNCH
            || item_id == ITEM_GRENADE
            || item_id == ITEM_THROWKNIFE
            || item_id == ITEM_REMOTEMINE
            || item_id == ITEM_PROXIMITYMINE
            || item_id == ITEM_TIMEDMINE
            || item_id == ITEM_FLAREPISTOL
            || item_id == ITEM_PITONGUN
            || item_id == ITEM_BOMBCASE
            || item_id == ITEM_BUG
            || item_id == ITEM_MICROCAMERA
            || item_id == ITEM_GOLDENEYEKEY
            || item_id == ITEM_TOKEN
            || item_id == ITEM_PLASTIQUE
        )
        {
            // nothing to do
        }
        else if (item_id == ITEM_TANKSHELLS)
        {
            gunFireTankShell(hand);
        }
        else if (item_id == ITEM_FIST || item_id == ITEM_KNIFE)
        {
            shotPunch(hand, item_id);
        }
        else if (item_id == ITEM_SHOTGUN || item_id == ITEM_AUTOSHOT)
        {
            statsAddHit(item_id, SHOT_REGISTER_TOTAL);

            for (i=0; i<NUMBER_SHOTGUN_BULLETS; i++)
            {
                shotFireBullet(hand);
            }
        }
        else if (item_id == ITEM_CAMERA)
        {
            objectivesCheckPicture();
        }
        else if (item_id == ITEM_WATCHMAGNETATTRACT)
        {
            g_CurrentPlayer->magnetattracttime = 0;
        }
        else
        {
            statsAddHit(item_id, SHOT_REGISTER_TOTAL);
            shotFireBullet(hand);
        }
    }
}


void shotFireHands(void)
{
    shotFireHand(GUNRIGHT);
    shotFireHand(GUNLEFT);
}


// Copied from another file. Might need code reorganization to prevent the use of extra externs.
extern ALSoundState* g_CasingSfxState;
extern ALSoundState* g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];

extern u32 g_GunDebApplyKeyframe;
extern u32 g_gunDebKeyframeIndex;

extern CartridgeModelFileRecord ejected_cartridge[] ;

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
