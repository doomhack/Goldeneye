/**
 * Things characters carry: the guns in their hands and the hats on their heads, and
 * dropping them.
 *
 * A held gun or hat is an object whose prop is a child of the character's prop, with its
 * model attached to a node of the character's model (switch 3 for the right hand, 5 for
 * the left, 6 for the head). Guns given during play take a weapon slot and hats a hat
 * slot (objslots.c).
 *
 * Dropping (objDrop) detaches the object and launches it as a projectile: a surrendering
 * guard lets go of their gun, a thrown grenade flies forwards, a shot-off hat flies away
 * from the player, and anything else tumbles in a random direction. The random tumble
 * numbers and the order of the float sums are kept exactly, as the demos depend on them.
 */

#include <ultra64.h>
#include <math.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <random.h>
#include "game/prop/carried.h"
#include "game/prop/objslots.h"
#include "game/prop/propobj.h"
#include "game/prop/projectile.h"
#include "game/prop/prop.h"
#include "game/prop/setup.h"
#include "game/chr/chr.h"
#include "game/chr/chraction.h"
#include "game/gfx/dyn.h"
#include "game/math/matrix.h"
#include "game/math/physics.h"
#include "game/math/trig.h"
#include "game/model/chrobjdata.h"
#include "game/model/jointlist.h"
#include "game/model/model.h"
#include "game/nav/stan.h"
#include "game/player/bondinv.h"
#include "game/player/player.h"

/* The model switches that held guns and hats attach to */
#define CHR_SWITCH_RIGHTHAND 3
#define CHR_SWITCH_LEFTHAND  5
#define CHR_SWITCH_HEAD      6

/* A flash of muzzle fire and its light, in weapon models */
#define WEAPON_SWITCH_GUNFIRE      0
#define WEAPON_SWITCH_GUNFIRELIGHT 2

/* Random spins given to dropped things, in radians per frame. The PAL version spins 6/5
   as fast per frame, to make up for running at 50 frames a second. */
#ifdef VERSION_EU
#define TUMBLE_TURN              7.53982257843f
#define TUMBLE_SMALL_OFFSET      0.029452431947f
#define TUMBLE_LARGE_OFFSET      0.117809727788f
#else
#define TUMBLE_TURN              M_TAU_F
#define TUMBLE_SMALL_OFFSET      0.024543693f
#define TUMBLE_LARGE_OFFSET      0.09817477f
#endif
#define TUMBLE_SMALL_FRACTION    0.0078125f
#define TUMBLE_LARGE_FRACTION    0.03125f


/* Setup files store an object's health as a whole number in 1/65536ths. Convert it in
   place to a float when the object is first set up. */
static void objConvertSetupHealth(ObjectRecord *obj)
{
    union { f32 f; s32 i; } health = { .f = obj->health };

    obj->health = health.i / M_U16_MAX_VALUE_F;
}


static void modelApplyExtraScale(Model *model, s32 extrascale)
{
    f32 scale = extrascale * (1.0f / 256.0f);

    modelSetScale(model, model->scale * scale);
}


static PropRecord *hatApplyToChr(HatRecord *hat, ChrRecord *chr, ModelFileHeader *modeldef, PropRecord *prop, Model *model)
{
    prop = objInit((ObjectRecord *)hat, modeldef, prop, model);

    if (prop && hat->model)
    {
        modelApplyExtraScale(hat->model, hat->extrascale);
        hat->model->attachedto = chr->model;
        hat->model->attachedto_objinst = chr->model->obj->Switches[CHR_SWITCH_HEAD];

        propReparent(prop, chr->prop);
        chr->handle_positiondata_hat = prop;
    }

    return prop;
}


/* Put a hat from the setup file on its guard */
void hatAssignToChr(HatRecord *hat, ChrRecord *chr)
{
    s32 modelnum = (u32)hat->obj;

    objConvertSetupHealth((ObjectRecord *)hat);
    modelLoad(modelnum);
    hatApplyToChr(hat, chr, PitemZ_entries[modelnum].header, NULL, NULL);
}


/**
 * Give a guard a new hat during play, from the hat slots. Returns its prop, or NULL if
 * no prop, model or slot could be found.
 */
PropRecord *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags)
{
    ModelFileHeader *modeldef = PitemZ_entries[modelnum].header;
    PropRecord *prop;
    Model *model;
    HatRecord *hat;

    modelLoad(modelnum);
    prop = propAllocate();
    model = modelmgrInstantiateModel(modeldef);

    // If there was no free prop or model, the hat taken frees one up
    hat = hatCreate(prop == NULL, model == NULL, modeldef);

    if (prop == NULL)
    {
        prop = propAllocate();
    }

    if (model == NULL)
    {
        model = modelmgrInstantiateModel(modeldef);
    }

    if (hat == NULL || prop == NULL || model == NULL)
    {
        if (model)
        {
            modelFreeSlot(model);
        }

        if (prop)
        {
            propFree(prop);
        }

        return NULL;
    }

    *hat = (HatRecord){
        .extrascale = 0x0100,
        .type       = PROPDEF_HAT,
        .obj        = modelnum,
        .pad        = chr->chrnum,
        .flags      = flags | PROPFLAG_ASSIGNEDTOCHR,
        .mtx        = {.m = {{1.0f, 0.0f, 0.0f, 0.0f},
                             {0.0f, 1.0f, 0.0f, 0.0f},
                             {0.0f, 0.0f, 1.0f, 0.0f},
                             {0.0f, 0.0f, 0.0f, 1.0f}}},
        .health     = 1000.0f,
        .shadecol   = {{0xFF, 0xFF, 0xFF, 0x00}},
        .nextcol    = {{0xFF, 0xFF, 0xFF, 0x00}},
    };

    return hatApplyToChr(hat, chr, modeldef, prop, model);
}


/* Which kind of hat a hat prop is, for what happens when it is shot */
HATTYPE hatGetModel(PropRecord *prop)
{
    switch (prop->obj->obj)
    {
        case PROP_HATFURRY:
        case PROP_HATFURRYBROWN:
        case PROP_HATFURRYBLACK:
            return HATTYPE_FURRY;

        case PROP_HATTBIRD:
        case PROP_HATTBIRDBROWN:
            return HATTYPE_BIRD;

        case PROP_HATHELMET:
        case PROP_HATHELMETGREY:
            return HATTYPE_HELMATE;

        case PROP_HATMOON:
            return HATTYPE_MOON;

        case PROP_HATBERET:
        case PROP_HATBERETBLUE:
        case PROP_HATBERETRED:
            return HATTYPE_BERRET;

        case PROP_HATPEAKED:
            return HATTYPE_PEAKED;

        default:
            return HATTYPE_OTHER;
    }
}


/* Show or hide a weapon model's muzzle flash */
void weaponSetGunfireVisible(PropRecord *prop, s32 firing)
{
    Model *model = prop->obj->model;
    ModelNode *node;

    if (model == NULL || model->obj->Skeleton != &skeleton_prop_weapon)
    {
        return;
    }

    node = model->obj->Switches[WEAPON_SWITCH_GUNFIRE];

    if (node)
    {
        modelGetNodeRwData(model, node)->Gunfire.visible = firing;
    }

    node = model->obj->Switches[WEAPON_SWITCH_GUNFIRELIGHT];

    if (node)
    {
        modelGetNodeRwData(model, node)->BSP.visible = firing;
    }
}


static PropRecord *weaponInit(WeaponObjRecord *weapon, ModelFileHeader *modeldef, PropRecord *prop, Model *model)
{
    prop = objInit((ObjectRecord *)weapon, modeldef, prop, model);

    if (prop != NULL)
    {
        prop->type = PROP_TYPE_WEAPON;
        weaponSetGunfireVisible(prop, FALSE);
    }

    return prop;
}


PropRecord *weaponInitWithModelDef(ObjectRecord *obj, ModelFileHeader *modeldef)
{
    return weaponInit((WeaponObjRecord *)obj, modeldef, NULL, NULL);
}


/* Make two weapons a pair, held one in each hand */
void weaponSetDual(WeaponObjRecord *leftweapon, WeaponObjRecord *rightweapon)
{
    leftweapon->LinkedWeaponType  = rightweapon->weaponnum;
    leftweapon->dualweapon        = rightweapon;
    rightweapon->LinkedWeaponType = leftweapon->weaponnum;
    rightweapon->dualweapon       = leftweapon;
}


/**
 * Put a weapon in a guard's hand: its right hand, or the left if the weapon is flagged
 * left-handed. A concealed weapon is carried but not held. Returns FALSE if that hand is
 * already full.
 */
bool chrEquipWeapon(WeaponObjRecord *weapon, ChrRecord *chr)
{
    GUNHAND hand = (weapon->flags & PROPFLAG_WEAPON_LEFTHANDED) ? GUNLEFT : GUNRIGHT;

    if (weapon->prop == NULL || weapon->model == NULL)
    {
        return TRUE;
    }

    if (!(weapon->flags & PROPFLAG_CONCEAL_GUN))
    {
        PropRecord *otherhand = chr->weapons_held[1 - hand];

        if (chr->weapons_held[hand])
        {
            return FALSE;
        }

        weapon->model->attachedto = chr->model;
        weapon->model->attachedto_objinst =
            chr->model->obj->Switches[hand == GUNRIGHT ? CHR_SWITCH_RIGHTHAND : CHR_SWITCH_LEFTHAND];

        chr->weapons_held[hand] = weapon->prop;

        if ((weapon->flags & PROPFLAG_IS_DOUBLE) && otherhand)
        {
            weaponSetDual(weapon, otherhand->weapon);
        }
    }

    propReparent(weapon->prop, chr->prop);

    return TRUE;
}


static PropRecord *chrInitHeldWeapon(WeaponObjRecord *weapon, ChrRecord *chr, ModelFileHeader *modeldef, PropRecord *prop, Model *model)
{
    prop = weaponInit(weapon, modeldef, prop, model);

    if (prop && weapon->model)
    {
        modelApplyExtraScale(weapon->model, weapon->extrascale);
        chrEquipWeapon(weapon, chr);
    }

    return prop;
}


/* Put a weapon from the setup file in its guard's hand */
void chrSetupHeldWeapon(WeaponObjRecord *weapon, ChrRecord *chr)
{
    s32 modelnum = weapon->obj;

    objConvertSetupHealth((ObjectRecord *)weapon);
    modelLoad(modelnum);
    chrInitHeldWeapon(weapon, chr, PitemZ_entries[modelnum].header, NULL, NULL);
}


/* A new weapon object, as a gun pickup with no setup flags */
static void weaponReset(WeaponObjRecord *weapon, s16 pad)
{
    *weapon = (WeaponObjRecord){
        .extrascale       = 0x0100,
        .type             = PROPDEF_COLLECTABLE,
        .pad              = pad,
        .mtx              = {.m = {{1.0f, 0.0f, 0.0f, 0.0f},
                                   {0.0f, 1.0f, 0.0f, 0.0f},
                                   {0.0f, 0.0f, 1.0f, 0.0f},
                                   {0.0f, 0.0f, 0.0f, 1.0f}}},
        .health           = 1000.0f,
        .shadecol         = {{0xFF, 0xFF, 0xFF, 0x00}},
        .nextcol          = {{0xFF, 0xFF, 0xFF, 0x00}},
        .weaponnum        = ITEM_UNARMED,
        .LinkedWeaponType = -1,
        .timer            = -1,
    };
}


/**
 * Make a loose weapon object during play (a thrown grenade, a placed mine, a fired
 * rocket), from the weapon slots.
 *
 * @param modelnum: index into PitemZ_entries (enum PROP)
 * @param weaponid: the weapon it is (ITEM_IDS)
 */
WeaponObjRecord *weaponCreateFromModel(PROP modelnum, s32 weaponid)
{
    ModelFileHeader *modeldef = PitemZ_entries[modelnum].header;
    PropRecord *prop;
    Model *model;
    WeaponObjRecord *weapon;

    modelLoad(modelnum);
    prop = propAllocate();
    model = modelmgrInstantiateModel(modeldef);
    weapon = weaponCreate(prop == NULL, model == NULL, modeldef);

    if (prop == NULL)
    {
        prop = propAllocate();
    }

    if (model == NULL)
    {
        model = modelmgrInstantiateModel(modeldef);
    }

    if (weapon == NULL || prop == NULL || model == NULL)
    {
        if (model != NULL)
        {
            modelFreeSlot(model);
        }

        if (prop != NULL)
        {
            propFree(prop);
        }

        return NULL;
    }

    weaponReset(weapon, 1);
    weapon->weaponnum = weaponid;
    weapon->obj = modelnum;

    weaponInit(weapon, modeldef, prop, model);

    return weapon;
}


/**
 * Give a guard (or the player's body) a weapon in hand, from the weapon slots unless a
 * weapon object is given. Returns the weapon's prop, or NULL if none could be made.
 */
PropRecord *chrGiveWeaponEx(ChrRecord *chr, s32 modelnum, ITEM_IDS weaponid, s32 flags, WeaponObjRecord *weapon, ModelFileHeader *modeldef)
{
    PropRecord *prop;
    Model *model;

    if (!modeldef)
    {
        modeldef = PitemZ_entries[modelnum].header;
        modelLoad(modelnum);
    }

    prop = propAllocate();
    model = modelmgrInstantiateModel(modeldef);

    if (!weapon)
    {
        weapon = weaponCreate(prop == NULL, model == NULL, modeldef);
    }

    if (!prop)
    {
        prop = propAllocate();
    }

    if (!model)
    {
        model = modelmgrInstantiateModel(modeldef);
    }

    if (weapon == NULL || prop == NULL || model == NULL)
    {
        if (model)
        {
            modelFreeSlot(model);
        }

        if (prop)
        {
            propFree(prop);
        }

        return NULL;
    }

    weaponReset(weapon, 0x4001);
    weapon->weaponnum = weaponid;
    weapon->obj = modelnum;
    weapon->flags = flags | PROPFLAG_ASSIGNEDTOCHR;
    weapon->pad = chr->chrnum;

    return chrInitHeldWeapon(weapon, chr, modeldef, prop, model);
}


PropRecord *chrGiveWeapon(ChrRecord *chr, s32 modelnum, ITEM_IDS weaponid, s32 flags)
{
    return chrGiveWeaponEx(chr, modelnum, weaponid, flags, NULL, NULL);
}


/* Mark the weapon in a guard's hand to be removed */
void chrRemoveHeldWeapon(ChrRecord *chr, GUNHAND hand)
{
    if (chr->weapons_held[hand])
    {
        chr->weapons_held[hand]->weapon->runtime_bitflags |= RUNTIMEBITFLAG_REMOVE;
    }
}


/**
 * Work out the matrices of the gun in a guard's hand, and add it to the joint list for
 * drawing. The left hand's gun is turned over. A gun waiting to respawn or hidden isn't
 * drawn.
 */
void chrRenderHeldWeapon(PropRecord *chrprop, GUNHAND hand, JointListEntry **jointList)
{
    ChrRecord *chr = chrprop->chr;
    PropRecord *prop = chrGetEquippedWeaponProp(chr, hand);
    ObjectRecord *obj;
    Model *model;
    ModelRenderData renderdata;
    Mtxf lefthandmtx;

    if (prop == NULL)
    {
        return;
    }

    obj = prop->obj;

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_RESPAWNING) || (obj->flags2 & PROPFLAG2_00080000))
    {
        prop->flags &= ~PROPFLAG_ONSCREEN;
        return;
    }

    model = obj->model;
    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};

    prop->flags |= PROPFLAG_ONSCREEN;

    renderdata.basemtx = modelFindNodeMtx(chr->model, model->attachedto_objinst, 0);

    if (hand == GUNLEFT)
    {
        mtx4LoadZRotation(3.1415927f, &lefthandmtx);
        mtx4MultInPlace(renderdata.basemtx, &lefthandmtx);
        renderdata.basemtx = &lefthandmtx;
    }

    renderdata.mtxlist = dynAllocate(model->obj->numMatrices * sizeof(Mtxf));
    modelCalcMatricesNoAnim(&renderdata, model);

    if (jointList != NULL && !(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        *jointList = jointListAddModel(*jointList, model);
    }
}


/* Find a weapon of the given weapon number: the prop itself or any of its children */
static WeaponObjRecord *weaponFindInProp(s32 weaponnum, PropRecord *prop)
{
    if (prop->type == PROP_TYPE_WEAPON && prop->weapon->weaponnum == weaponnum)
    {
        return prop->weapon;
    }

    for (PropRecord *child = prop->child; child; child = child->prev)
    {
        WeaponObjRecord *weapon = weaponFindInProp(weaponnum, child);

        if (weapon)
        {
            return weapon;
        }
    }

    return NULL;
}


/**
 * Find a weapon of the given weapon number lying in the level (not in flight as a
 * projectile). The AI command "if key dropped" uses this for keys and other items.
 */
WeaponObjRecord *weaponFindThrown(s32 weaponnum)
{
    for (PropRecord *prop = propsGetActiveTail(); prop; prop = prop->prev)
    {
        WeaponObjRecord *weapon = weaponFindInProp(weaponnum, prop);

        if (weapon && !(weapon->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
        {
            return weapon;
        }
    }

    return NULL;
}


/* Say how a carried object will be dropped (DROPTYPE), giving it a projectile for the
   flight */
void objSetDropType(PropRecord *prop, DROPTYPE droptype)
{
    ObjectRecord *obj = prop->obj;

    if (prop->parent == NULL)
    {
        return;
    }

    objEnsureProjectile(prop);

    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) && obj->embedment->projectile)
    {
        obj->embedment->projectile->droptype = droptype;
    }
    else if (obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
    {
        obj->projectile->droptype = droptype;
    }
}


/* Take a carried object off whatever carries it. A guard's hat or held gun is taken from
   them, and a gun stops firing. */
void objDetach(PropRecord *prop)
{
    PropRecord *parent = prop->parent;
    ObjectRecord *obj = prop->obj;

    if (parent == NULL)
    {
        return;
    }

    propDetach(prop);

    obj->model->attachedto_objinst = NULL;
    obj->runtime_bitflags &= ~RUNTIMEBITFLAG_HASOWNER;

    if ((parent->type == PROP_TYPE_CHR || parent->type == PROP_TYPE_VIEWER) && parent->chr)
    {
        ChrRecord *chr = parent->chr;

        if (prop == chr->handle_positiondata_hat)
        {
            chr->handle_positiondata_hat = NULL;
        }
        else if (prop == chr->weapons_held[GUNRIGHT])
        {
            chrSetFiring(chr, GUNRIGHT, FALSE);
            chr->weapons_held[GUNRIGHT] = NULL;
        }
        else if (prop == chr->weapons_held[GUNLEFT])
        {
            chrSetFiring(chr, GUNLEFT, FALSE);
            chr->weapons_held[GUNLEFT] = NULL;
        }
    }
}


/* Give a dropped object a random spin */
static void projectileRandomTumble(Projectile *projectile, f32 fraction, f32 offset)
{
    coord3d rot = { {0.0f, 0.0f, 0.0f} };

    rot.x = (RANDOMFRAC() * TUMBLE_TURN * fraction) - offset;
    rot.y = (RANDOMFRAC() * TUMBLE_TURN * fraction) - offset;
    rot.z = (RANDOMFRAC() * TUMBLE_TURN * fraction) - offset;

    mtx4LoadRotation(&rot, &projectile->mtx);
}


/* The speed and spin a dropped object starts its flight with */
static void objLaunchDropped(Projectile *projectile, PropRecord *parent)
{
    if (projectile->droptype == DROPTYPE_SURRENDER && parent->type == PROP_TYPE_CHR)
    {
        // Let go of in front of the guard
        f32 angle = modelGetRootRotY(parent->chr->model);

        projectile->speed.x = sinf(angle) * 1.6666666f;
        projectile->speed.y = -RANDOMFRAC() * 1.6666666f * 0.5f;
        projectile->speed.z = cosf(angle) * 1.6666666f;

        projectileRandomTumble(projectile, TUMBLE_SMALL_FRACTION, TUMBLE_SMALL_OFFSET);
    }
    else if (projectile->droptype == DROPTYPE_THROWGRENADE && parent->type == PROP_TYPE_CHR)
    {
        // Thrown forwards and up
        f32 angle = modelGetRootRotY(parent->chr->model);

        projectile->speed.x = sinf(angle) * 13.333333f;
        projectile->speed.y = 6.6666665f;
        projectile->speed.z = cosf(angle) * 13.333333f;

        projectileRandomTumble(projectile, TUMBLE_SMALL_FRACTION, TUMBLE_SMALL_OFFSET);
        projectile->flags |= PROJECTILEFLAG_TESTHEIGHT;
    }
    else if (projectile->droptype == DROPTYPE_HAT)
    {
        // Knocked away from the player
        PropRecord *playerprop = currentPlayerGetProp();
        f32 x = parent->pos.x - playerprop->pos.x;
        f32 z = parent->pos.z - playerprop->pos.z;
        f32 angle = mathAtan2(x, z);

        projectile->speed.x = ((2.0f * (RANDOMFRAC() * 1.6666666f)) + 3.3333333f) * sinf(angle);
        projectile->speed.y = 2.0f * (RANDOMFRAC() * 1.6666666f);
        projectile->speed.z = ((2.0f * (RANDOMFRAC() * 1.6666666f)) + 3.3333333f) * cosf(angle);

        projectileRandomTumble(projectile, TUMBLE_LARGE_FRACTION, TUMBLE_LARGE_OFFSET);
    }
    else
    {
        physRandomLaunch(&projectile->speed, &projectile->mtx);
    }
}


/**
 * Drop a carried object: detach it and launch it from where it is. Its drop type
 * (objSetDropType) says how it is launched. Returns TRUE if it was dropped, FALSE if it
 * isn't carried or has no projectile.
 */
bool objDrop(PropRecord *prop)
{
    PropRecord *parent = prop->parent;
    ObjectRecord *obj = prop->obj;
    Projectile *projectile;
    Model *model;
    Mtxf mtx;
    PropRecord *root;
    StandTile *stan;

    // Stuck in something: the embedment's projectile becomes the object's own
    if ((obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) && obj->embedment->projectile)
    {
        Projectile *embedded = obj->embedment->projectile;

        embedmentFree(obj->embedment);

        obj->projectile = embedded;
        obj->runtime_bitflags &= ~RUNTIMEBITFLAG_EMBEDDED;
        obj->runtime_bitflags |= RUNTIMEBITFLAG_HASPROJECTILE;
    }

    if (parent == NULL || !(obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE))
    {
        return FALSE;
    }

    model = obj->model;
    projectile = obj->projectile;

    projectile->flags |= PROJECTILEFLAG_AIRBORNE;
    projectile->ownerprop = parent;

    objLaunchDropped(projectile, parent);

    for (root = parent; root->parent != NULL; root = root->parent)
    {
    }

    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        // Start where the model was drawn, if that point can be reached from the carrier
        // and the object fits there. Otherwise start at the carrier's position.
        f32 width = objGetWidth(obj);
        Mtxf *rendermtx = modelGetRootMtx(model);
        s32 cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;

        mtx4MultAffine(camGetViewToWorldMtxf(), rendermtx, &mtx);

        if (projectile->flags & PROJECTILEFLAG_TESTHEIGHT)
        {
            cdtypes = CDTYPE_OBJS | CDTYPE_PLAYERS | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
        }

        stan = root->stan;
        propSetMoving(root, FALSE);

        if (stanTestLineUnobstructed(&stan, root->pos.f[0], root->pos.f[2], mtx.m[3][0], mtx.m[3][2], cdtypes, 0.0f, 1.0f, 0.0f, 1.0f)
                && stanTestVolume(&stan, mtx.m[3][0], mtx.m[3][2], width, cdtypes, 0.0f, 1.0f) < 0)
        {
            prop->stan = stan;
        }
        else
        {
            prop->stan = root->stan;
            mtx.m[3][0] = root->pos.x;
            mtx.m[3][2] = root->pos.z;
        }

        propSetMoving(root, TRUE);
        prop->zDepth = -rendermtx->m[3][2];
    }
    else
    {
        // Not drawn this frame, so start at the carrier's position without any collision
        // tests (which lets speedrunners throw mines through doors)
        prop->stan = root->stan;
        mtx4LoadIdentity(&mtx);
        mtx4ScaleRows012(model->scale, &mtx);
        mtx4SetTranslation(&root->pos, &mtx);
    }

    objDetach(prop);
    propActivate(prop);
    propEnable(prop);

    obj->runtime_pos.x = prop->pos.x = mtx.m[3][0];
    obj->runtime_pos.y = prop->pos.y = mtx.m[3][1];
    obj->runtime_pos.z = prop->pos.z = mtx.m[3][2];

    mtx.m[3][0] = 0.0f;
    mtx.m[3][1] = 0.0f;
    mtx.m[3][2] = 0.0f;

    mtx4Copy(&mtx, &obj->mtx);

    objGetShadeColour(obj->prop, &obj->nextcol);
    obj->shadecol = obj->nextcol;

    setupUpdateObjectRoomPosition(obj);

    return TRUE;
}


/* The player drops everything they carry (multiplayer, on death) */
void playerDropInventory(void)
{
    ChrRecord *playerchr = g_CurrentPlayer->prop->chr;

    chrRemoveHeldWeapon(playerchr, GUNRIGHT);
    chrRemoveHeldWeapon(playerchr, GUNLEFT);

    for (ITEM_IDS item = ITEM_FIST; item != ITEM_IDS_MAX; item++)
    {
        PROP modelnum = weaponGetChrModel(item);

        if (modelnum >= 0 && invHasItem(item))
        {
            PropRecord *prop = chrGiveWeaponEx(playerchr, modelnum, item, PROPFLAG_CONCEAL_GUN, NULL, NULL);

            if (prop != NULL)
            {
                objSetDropType(prop, DROPTYPE_DEFAULT);
                objDrop(prop);
            }
        }
    }
}
