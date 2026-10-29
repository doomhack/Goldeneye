/**
 * Doors: sliding, vertical, swinging, the Caverns eye and iris doors, the fall-away grates
 * and the Aztec chair (which moves like a swinging door).
 *
 * A door's openPosition goes from 0 (shut) to maxFrac: a distance for sliding doors, an
 * angle in degrees for swinging ones. Doors can be linked in a ring (linkedDoor), and the
 * whole ring moves together. Each tick (doorTick) a door closes itself once it has been open
 * for autoCloseFrames, then moves; if a moving door would hit something, the whole ring
 * goes back to where it was.
 *
 * Each door has a collision outline (built from its model's bounding box, doorUpdateCollision)
 * that it loses once it is open past perimFrac, a portal it opens and closes so the rooms
 * behind it are drawn only when they can be seen, and up to three sounds per move (a start
 * sound, a loop while moving and a stop sound) picked by its soundType.
 */

#include <ultra64.h>
#include <assert.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <macro.h>
#include <memp.h>
#include <music.h>
#include <snd.h>
#include "game/audio/sfxvolume.h"
#include "game/chr/chrai.h"
#include "game/gfx/dyn.h"
#include "game/level/lv.h"
#include "game/math/matrix.h"
#include "game/math/trig.h"
#include "game/model/chrobjdata.h"
#include "game/model/model.h"
#include "game/player/bondinv.h"
#include "game/player/bondview.h"
#include "game/player/player.h"
#include "game/prop/door.h"
#include "game/prop/explosion.h"
#include "game/prop/prop.h"
#include "game/prop/propobj.h"
#include "game/text/language.h"
#include "game/world/bg.h"
#include "assets/obseg/text/LpropobjE.h"

static_assert(__builtin_offsetof(DoorRecord, loopSounds) == 0xf4, "DoorRecord layout");
static_assert(sizeof(DoorRecord) == 0x100, "DoorRecord layout");

/* The player can open a door when both its edges are within this angle of where they look (20 degrees) */
#define DOOR_INTERACT_ANGLE 0.34906587f

/* How close the player must be to a door to open it: 2 metres across, and 2 metres up or down */
#define DOOR_REACH_SQ 40000.0f
#define DOOR_REACH_Y 200.0f
#define DOOR_REACH_PAD_MARGIN 150.0f

/* Each door and the doors linked to it, in a ring through linkedDoor */
#define FOR_EACH_LINKED_DOOR(sibling, door) \
    for (sibling = (door); sibling != NULL; sibling = (sibling->linkedDoor == (door)) ? NULL : sibling->linkedDoor)

static f32 degreesToRadians(f32 degrees)
{
    return (degrees * M_TAU_F) / 360.0f;
}

/** The pad's side axis: across the door, at right angles to up and look. */
static void padGetSide(const BoundPadRecord *pad, coord3d *side)
{
    side->x = (pad->up.y * pad->look.z) - (pad->up.z * pad->look.y);
    side->y = (pad->up.z * pad->look.x) - (pad->up.x * pad->look.z);
    side->z = (pad->up.x * pad->look.y) - (pad->up.y * pad->look.x);
}


/*
 * Sounds
 */

/* The sounds for each soundType. 0 (NOTHING_SFX) means no sound. */
struct DoorSounds
{
    s16 lead;    /* a second sound with the start sound */
    s16 start;   /* as the door starts to move */
    s16 loop;    /* while it moves */
    s16 stop;    /* when it stops */
    bool hinged; /* only the start sound when it opens, and the stop sound when it shuts */
};

static const struct DoorSounds s_DoorSounds[] = {
    [DOOR_OPEN_SOUND_01] = {0, DOOR_SMART_CATCH1_SFX, DOOR_SMART_SLIDE1_SFX, DOOR_SMART_CATCH1_SFX},
    [DOOR_OPEN_SOUND_02] = {0, DOOR_SMART_CATCH1_SFX, TRAIN_SLIDE_DOOR_SLIDE_SFX, DOOR_SMART_CATCH1_SFX},
    [DOOR_OPEN_SOUND_METAL] = {0, METAL_SLIDE_OPEN_SFX, METAL_SLIDE_LOOP_SFX, METAL_SLIDE_CLOSE_SFX},
    [DOOR_OPEN_SOUND_04] = {0, HEAVY_SLIDE_OPEN_SFX, HEAVY_SINGLE_LOOP_SFX, HEAVY_SLIDE_CLOSE_SFX},
    [DOOR_OPEN_SOUND_WOOD] = {0, DOOR_WOOD_OPEN_SFX, 0, DOOR_WOOD_CLOSE_SFX, TRUE},
    [DOOR_OPEN_SOUND_06] = {0, TRAIN_SLIDE_DOOR_SLIDE_SFX, 0, TRAIN_SLIDE_DOOR_SLIDE_SFX, TRUE},
    [DOOR_OPEN_SOUND_WOOD_2] = {0, TRAIN_SLIDE_DOOR_CATCH_SFX, DOOR_WOOD_SLIDE_SFX, DOOR_SMART_CATCH1_SFX},
    [DOOR_OPEN_SOUND_WOOD_3] = {0, DOOR_WOOD_OPEN_SFX, TRAIN_SLIDE_DOOR_SLIDE_SFX, DOOR_WOOD_CLOSE_SFX},
    [DOOR_OPEN_SOUND_09] = {0, 0, DOOR_SHUTTER_OPEN_SFX, DOOR_SHUTTER_CLOSE_SFX},
    [DOOR_OPEN_SOUND_METAL_2] = {0, DOOR_METAL_OPEN_SFX, 0, DOOR_METAL_CLOSE_SFX, TRUE},
    [DOOR_OPEN_SOUND_11] = {0, TRAIN_SLIDE_DOOR_SLIDE_SFX, 0, DOOR_METAL_CLOSE2_SFX, TRUE},
    [DOOR_OPEN_SOUND_METAL_3] = {0, DOOR_METAL_OPEN3_SFX, 0, DOOR_METAL_CLOSE3_SFX, TRUE},
    [DOOR_OPEN_SOUND_13] = {0, TRAIN_SLIDE_DOOR_SLIDE_SFX, TRAIN_SLIDE_DOOR_SLIDE_SFX, TRAIN_SLIDE_DOOR_SLIDE_SFX},
    [DOOR_OPEN_SOUND_HYDROLIC] = {0, 0, DOOR_HYDRAL_CLOSE_SFX, DOOR_HYDRAL_OPEN_SFX},
    [DOOR_OPEN_SOUND_STONE] = {0, 0, DOOR_SLIDE_STONE_OPEN_SFX, DOOR_SLIDE_STONE_CLOSE_SFX},
    [DOOR_OPEN_SOUND_16] = {0, HEAVY_SLIDE_OPEN_SFX, 0, HEAVY_SLIDE_CLOSE_SFX},
    [DOOR_OPEN_SOUND_METAL_4] = {TRAIN_SLIDE_DOOR_SLIDE_SFX, METAL_SLIDE_OPEN_SFX, METAL_SLIDE_LOOP_SFX, METAL_SLIDE_CLOSE_SFX},
    [DOOR_OPEN_SOUND_18] = {0},
};

enum DoorSoundEvent
{
    DOORSOUND_OPENING,
    DOORSOUND_CLOSING,
    DOORSOUND_OPENED,
    DOORSOUND_CLOSED
};

static ALSoundState *doorPlaySfx(s16 sfx)
{
    if (sfx == NOTHING_SFX)
    {
        return NULL;
    }

    return sndPlaySfx(g_musicSfxBufferPtr, sfx, NULL);
}

static bool doorSoundIsPlaying(ALSoundState *sound)
{
    return sound != NULL && sndGetPlayingState(sound) != 0;
}

/** Turns the door's loops up or down for how near the nearest player is, and off in cutscenes. */
void doorUpdateSoundVolume(DoorRecord *door)
{
    bool playing[2];
    s32 volume;
    s32 i;

    for (i = 0; i < 2; i++)
    {
        playing[i] = doorSoundIsPlaying(door->loopSounds[i]);
    }

    if (playing[0] || playing[1])
    {
        volume = sfxVolumeAt(&door->prop->pos);

        if (lvGetControlsLocked())
        {
            volume = 0;
        }

        for (i = 0; i < 2; i++)
        {
            if (playing[i])
            {
                sndCreatePostEvent(door->loopSounds[i], AL_SNDP_VOL_EVT, volume);
            }
        }
    }
}

void doorStopSounds(DoorRecord *door)
{
    for (s32 i = 0; i < 2; i++)
    {
        if (doorSoundIsPlaying(door->loopSounds[i]))
        {
            sndDeactivate(door->loopSounds[i]);
        }
    }
}

static void doorPlaySound(DoorRecord *door, enum DoorSoundEvent event)
{
    static const struct DoorSounds silent = {0};
    const struct DoorSounds *sounds = &silent;
    ALSoundState *sound = NULL;

    if (door->soundType < (u32)ARRAYCOUNT(s_DoorSounds))
    {
        sounds = &s_DoorSounds[door->soundType];
    }

    doorStopSounds(door);

    if (event == DOORSOUND_OPENING || (event == DOORSOUND_CLOSING && !sounds->hinged))
    {
        ALSoundState **loopSlot = NULL;

        /* The loop goes in a free slot. A stopped sound frees its slot only when it ends. */
        if (door->loopSounds[0] == NULL)
        {
            loopSlot = &door->loopSounds[0];
        }
        else if (door->loopSounds[1] == NULL)
        {
            loopSlot = &door->loopSounds[1];
        }

        if (sounds->lead != NOTHING_SFX)
        {
            sound = doorPlaySfx(sounds->lead);

            if (sound != NULL)
            {
                sfxSetVolumeAt(sound, &door->prop->pos);
            }
        }

        sound = doorPlaySfx(sounds->start);

        if (loopSlot != NULL && sounds->loop != NOTHING_SFX)
        {
            sndPlaySfx(g_musicSfxBufferPtr, sounds->loop, loopSlot);
        }
    }
    else if (event == DOORSOUND_CLOSED || (event == DOORSOUND_OPENED && !sounds->hinged))
    {
        sound = doorPlaySfx(sounds->stop);
    }

    if (sound != NULL)
    {
        sfxSetVolumeAt(sound, &door->prop->pos);
    }

    doorUpdateSoundVolume(door);
}


/*
 * Shape and position
 */

/** The door's model matrix in the world, with it as far open as it is. */
void doorGetMtx(DoorRecord *door, Mtxf *mtx)
{
    if (door->doorType == DOORTYPE_SWINGING || door->doorType == DOORTYPE_AZTECCHAIR)
    {
        BoundPadRecord *pad = &g_CurrentSetup.boundpads[door->pad];
        coord3d side;
        coord3d hinge;
        coord3d offset;
        Mtxf tmp;
        f32 hingeX;
        f32 angle;

        padGetSide(pad, &side);

        /* The hinge is at the bottom of one side: the far side for a chair or a door that opens to the front */
        hinge.x = pad->pos.x + (pad->up.x * pad->bbox.ymin);
        hinge.y = pad->pos.y + (pad->up.y * pad->bbox.ymin);
        hinge.z = pad->pos.z + (pad->up.z * pad->bbox.ymin);

        if (door->doorType == DOORTYPE_AZTECCHAIR || (door->flags & PROPFLAG_DOOR_OPENTOFRONT))
        {
            hingeX = pad->bbox.xmax;
        }
        else
        {
            hingeX = pad->bbox.xmin;
        }

        hinge.x += side.x * hingeX;
        hinge.y += side.y * hingeX;
        hinge.z += side.z * hingeX;

        offset.x = door->runtime_pos.x - hinge.x;
        offset.y = door->runtime_pos.y - hinge.y;
        offset.z = door->runtime_pos.z - hinge.z;

        /* Turn the door about the hinge: about the vertical for a door, the pad's look axis for the chair */
        mtx4Copy(&door->mtx, mtx);
        mtx4LoadTranslation(&offset, &tmp);
        mtx4MultInPlace(&tmp, mtx);

        angle = degreesToRadians(door->openPosition);

        if (door->flags & PROPFLAG_DOOR_OPENTOFRONT)
        {
            angle = M_TAU_F - angle;
        }

        if (door->doorType == DOORTYPE_AZTECCHAIR)
        {
            mtx4LoadZRotation(angle, &tmp);
        }
        else
        {
            mtx4LoadYRotation(angle, &tmp);
        }

        mtx4MultInPlace(&tmp, mtx);
        mtx4LoadTranslation(&hinge, &tmp);
        mtx4MultInPlace(&tmp, mtx);
    }
    else if (door->doorType == DOORTYPE_EYE || door->doorType == DOORTYPE_IRIS)
    {
        /* These open by moving parts of the model (doorGetModelMatrices) */
        mtx4Copy(&door->mtx, mtx);
        mtx4SetTranslation(&door->runtime_pos, mtx);
    }
    else
    {
        coord3d pos;

        pos.x = (door->slideDir.x * door->openPosition) + door->runtime_pos.x;
        pos.y = (door->slideDir.y * door->openPosition) + door->runtime_pos.y;
        pos.z = (door->slideDir.z * door->openPosition) + door->runtime_pos.z;

        mtx4Copy(&door->mtx, mtx);
        mtx4SetTranslation(&pos, mtx);
    }

    if (door->doorFlags & DOORFLAG_FLIP)
    {
        mtx4ScaleRow2(-1.0f, mtx);
    }
}

/**
 * Updates the door's bounding box and its collision outline for where it is. A door that
 * clips to its bounding box (DOORFLAG_CLIP_TO_BBOX) shrinks as it slides into the wall.
 */
static void doorUpdateCollision(DoorRecord *door)
{
    const struct ModelRoData_BoundingBoxRecord *modelbbox = &door->model->obj->RootNode->Child->Data->BoundingBox;
    struct collision_data *col = door->ptr_allocated_collisiondata_block;
    Mtxf mtx;

    door->bbox = *modelbbox;

    if (door->doorFlags & DOORFLAG_CLIP_TO_BBOX)
    {
        if (door->doorType == DOORTYPE_VERTICAL)
        {
            door->bbox.Bounds.ymax = modelbbox->Bounds.ymax + (modelbbox->Bounds.ymin - modelbbox->Bounds.ymax) * door->openPosition;
        }
        else
        {
            door->bbox.Bounds.xmin = modelbbox->Bounds.xmin + (modelbbox->Bounds.xmax - modelbbox->Bounds.xmin) * door->openPosition;
        }
    }

    /* Open far enough to walk through */
    if (door->openPosition >= door->perimFrac)
    {
        col->edges = 0;
        return;
    }

    doorGetMtx(door, &mtx);
    col->edges = bboxGetHull2d(&door->bbox, &mtx, col->polygon);

    if (door->doorType == DOORTYPE_VERTICAL)
    {
        col->bottom = door->runtime_pos.y + bboxRotatedMinY(&door->bbox, &mtx);
    }
    else if (door->doorType == DOORTYPE_FALLAWAY)
    {
        col->bottom = door->runtime_pos.y - 10000.0f;
    }
    else
    {
        col->bottom = mtx.m[3][1] + bboxRotatedMinY(&door->bbox, &mtx);

        if (door->doorFlags & DOORFLAG_EXTENDEDY)
        {
            col->bottom -= 1000.0f;
        }
    }

    /* An eye or iris door more than 40% open is only half a metre high: you can step through */
    if ((door->doorType == DOORTYPE_EYE || door->doorType == DOORTYPE_IRIS)
        && 0 < door->openPosition - (0.4f * door->maxFrac))
    {
        col->top = col->bottom + 50.0f;
    }
    else if (door->doorType == DOORTYPE_FALLAWAY)
    {
        col->top = door->runtime_pos.y + 1000.0f;
    }
    else
    {
        col->top = mtx.m[3][1] + bboxRotatedMaxY(&door->bbox, &mtx);

        if (door->doorFlags & DOORFLAG_EXTENDEDY)
        {
            col->top += 1000.0f;
        }
    }
}

/*
 * Moves a clipped corner's texture coordinate along its edge, so the texture stays where it
 * was on the part of the door still showing: `cut` of the edge's `length` is gone.
 */
static s16 clipTexCoord(s32 cut, s32 length, s16 corner, s16 other)
{
    return (cut * (other - corner) / length) + corner;
}

/*
 * Clips the corners of a quad that are past the cutoff. A vertical door is clipped from the
 * top (y above cutoff), a sliding door from the side (x below cutoff). Each clipped corner's
 * texture coordinates move along the edge to the corner straight across the door from it.
 */
static void doorClipQuad(const Vertex *src, Vertex *dst, s16 cutoff, bool vertical)
{
    for (s32 j = 0; j < 4; j++)
    {
        const Vertex *cur = &src[j];
        Vertex *out = &dst[j];

        for (s32 k = 1; k < 4; k++)
        {
            const Vertex *other = &src[(j + k) % 4];

            if (vertical && cur->coord.y >= cutoff)
            {
                if (cur->coord.x == other->coord.x && cur->coord.z == other->coord.z && cur->coord.y != other->coord.y)
                {
                    out->s = clipTexCoord(cur->coord.y - cutoff, cur->coord.y - other->coord.y, cur->s, other->s);
                    out->t = clipTexCoord(cur->coord.y - cutoff, cur->coord.y - other->coord.y, cur->t, other->t);
                    break;
                }
            }
            else if (!vertical && cur->coord.x <= cutoff)
            {
                if (cur->coord.y == other->coord.y && cur->coord.z == other->coord.z && cur->coord.x != other->coord.x)
                {
                    out->s = clipTexCoord(cutoff - cur->coord.x, other->coord.x - cur->coord.x, cur->s, other->s);
                    out->t = clipTexCoord(cutoff - cur->coord.x, other->coord.x - cur->coord.x, cur->t, other->t);
                    break;
                }
            }
        }

        if (vertical && cur->coord.y >= cutoff)
        {
            out->coord.y = cutoff;
        }
        else if (!vertical && cur->coord.x <= cutoff)
        {
            out->coord.x = cutoff;
        }
    }
}

/**
 * A door that clips to its bounding box (DOORFLAG_CLIP_TO_BBOX) has its model cut back to
 * the box as it moves, so it doesn't poke through the wall it slides into. This builds this
 * frame's clipped vertices. The model is made of quads, 4 vertices each.
 * Examples are the brown security doors in Facility and the stone doors in Egypt and Temple.
 */
static void doorClipVertices(DoorRecord *door)
{
    Model *model;
    ModelNode *node;
    const struct ModelRoData_DisplayList_CollisionRecord *rodata;
    struct ModelRwData_DisplayList_CollisionRecord *rwdata;
    bool vertical;
    s16 cutoff;

    if (!(door->doorFlags & DOORFLAG_CLIP_TO_BBOX))
    {
        return;
    }

    model = door->model;
    node = model->obj->RootNode->Child->Child;
    rodata = &node->Data->DisplayListCollisions;
    rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;
    vertical = door->doorType == DOORTYPE_VERTICAL;

    if (vertical)
    {
        cutoff = door->bbox.Bounds.ymax + 0.5f;
    }
    else
    {
        cutoff = door->bbox.Bounds.xmin + 0.5f;
    }

    rwdata->Vertices = dynAllocateVertices(rodata->numVertices);

    for (s32 i = 0; i < rodata->numVertices / 4; i++)
    {
        const Vertex *src = &rodata->Vertices[i * 4];
        Vertex *dst = &rwdata->Vertices[i * 4];

        for (s32 j = 0; j < 4; j++)
        {
            dst[j] = src[j];
        }

        doorClipQuad(src, dst, cutoff, vertical);
    }
}

/*
 * A clipped door's vertices from doorClipVertices last only a frame. Once it stops, they go in
 * the door's own copy (clipVtx) for the model to keep drawing.
 */
static void doorKeepClippedVertices(DoorRecord *door)
{
    Model *model = door->model;
    ModelNode *node = model->obj->RootNode->Child->Child;
    const struct ModelRoData_DisplayList_CollisionRecord *rodata = &node->Data->DisplayListCollisions;
    struct ModelRwData_DisplayList_CollisionRecord *rwdata = &modelGetNodeRwData(model, node)->DisplayListCollisions;

    if (rwdata->Vertices != door->clipVtx)
    {
        for (s32 i = 0; i < rodata->numVertices; i++)
        {
            door->clipVtx[i] = rwdata->Vertices[i];
        }
    }

    rwdata->Vertices = door->clipVtx;
}

/** Opens the door's portal, so the rooms behind it are drawn. */
void doorOpenPortal(DoorRecord *door)
{
    if (door->portalNumber >= 0)
    {
        bgToggleDataPortalsContrlBytes1Bit1(door->portalNumber, TRUE);
    }
}

void doorClosePortal(DoorRecord *door)
{
    if (door->portalNumber >= 0)
    {
        bgToggleDataPortalsContrlBytes1Bit1(door->portalNumber, FALSE);
    }
}

/**
 * Sets a door up at level load (from setupDoor), with its prop. The setup data gives where
 * it is (centre) and which way it slides (slideDir).
 */
PropRecord *doorInit(DoorRecord *door, coord3d *pos, Mtxf *mtx, StandTile *stan, coord3d *slideDir, coord3d *centre)
{
    PropRecord *prop = objInitWithAutoModel((ObjectRecord *)door);
    f32 scale = PitemZ_entries[door->obj].scale;

    /* Rounded up to 16 bytes, as memp doesn't */
    door->ptr_allocated_collisiondata_block = mempAlloc(ALIGN16_a(sizeof(struct collision_data)), MEMPOOL_STAGE);

    mtx4Copy(mtx, &door->mtx);
    mtx4ScaleRows012(scale, &door->mtx);

    door->slideDir = *slideDir;

    /* KEEPOPEN on a door in the setup means it starts open */
    if (door->flags & PROPFLAG_DOOR_KEEPOPEN)
    {
        door->openPosition = door->maxFrac;
    }
    else
    {
        door->openPosition = 0.0f;
    }

    door->speed = 0.0f;
    door->openstate = DOORSTATE_STATIONARY;
    door->numHits = 0;
    door->linkedDoor = NULL;

    if (door->doorFlags & DOORFLAG_CLIP_TO_BBOX)
    {
        union ModelRoData *rodata = door->model->obj->RootNode->Child->Child->Data;
        door->clipVtx = mempAlloc(rodata->DisplayListCollisions.numVertices * sizeof(Vertex), MEMPOOL_STAGE);
    }
    else
    {
        door->clipVtx = NULL;
    }

    door->portalNumber = -1;
    door->loopSounds[0] = NULL;
    door->loopSounds[1] = NULL;

    prop->type = PROP_TYPE_DOOR;
    prop->door = door;
    prop->pos = *pos;
    prop->stan = stan;

    door->runtime_pos = *centre;
    door->flags |= PROPFLAG_00000100;

    doorUpdateCollision(door);
    doorClipVertices(door);
    objGetShadeColour(door->prop, &door->nextcol);
    door->shadecol = door->nextcol;

    return prop;
}


/*
 * Opening and closing
 */

static void doorStartOpening(DoorRecord *door)
{
    door->flags &= ~PROPFLAG_DOOR_KEEPOPEN;
    door->runtime_bitflags |= RUNTIMEBITFLAG_BEENOPENED;

    doorPlaySound(door, DOORSOUND_OPENING);
    doorOpenPortal(door);

    /* A grate that falls away can't be shut again, and stops blocking at once */
    if (door->doorType == DOORTYPE_FALLAWAY)
    {
        struct collision_data *col = door->ptr_allocated_collisiondata_block;

        door->flags |= PROPFLAG_CANNOT_ACTIVATE;
        door->perimFrac = 0;

        if (col != NULL)
        {
            col->edges = 0;
        }

        door->flags &= ~PROPFLAG_00000100;
    }
}

static void doorStartClosing(DoorRecord *door)
{
    door->flags &= ~PROPFLAG_DOOR_KEEPOPEN;
    doorPlaySound(door, DOORSOUND_CLOSING);
}

static void doorFinishOpening(DoorRecord *door)
{
    doorPlaySound(door, DOORSOUND_OPENED);

    /* A grate falls from here as a projectile */
    if (door->doorType == DOORTYPE_FALLAWAY)
    {
        objEnsureProjectile(door->prop);

        if (door->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        {
            door->projectile->flags |= PROJECTILEFLAG_AIRBORNE;
            mtx4LoadIdentity(&door->projectile->mtx);
        }
    }
}

static void doorFinishClosing(DoorRecord *door)
{
    doorPlaySound(door, DOORSOUND_CLOSED);
    doorClosePortal(door);
}

/** Sets one door's state (not its linked doors'), starting its sounds as it starts to move. */
static void doorSetState(DoorRecord *door, DOORSTATE state)
{
    if (state == DOORSTATE_OPENING)
    {
        if (door->openstate == DOORSTATE_STATIONARY || door->openstate == DOORSTATE_WAITING)
        {
            doorStartOpening(door);
        }

        door->openstate = state;
    }
    else if (state == DOORSTATE_CLOSING)
    {
        if (door->openstate == DOORSTATE_STATIONARY && door->openPosition > 0)
        {
            doorStartClosing(door);
        }

        if ((door->openstate != DOORSTATE_STATIONARY && door->openstate != DOORSTATE_WAITING) || door->openPosition > 0)
        {
            door->openstate = state;
        }
        else if (door->openstate == DOORSTATE_WAITING)
        {
            /* Shut already, so it stops waiting to open */
            door->openstate = DOORSTATE_STATIONARY;
        }
    }
    else
    {
        door->openstate = state;
    }
}

/**
 * Opens or closes the door and the doors linked to it. Opening an airlock door closes the
 * linked doors instead, and the door waits until they have shut (doorTick).
 */
void doorMove(DoorRecord *door, DOORSTATE state)
{
    DOORSTATE linkedState = state;
    DoorRecord *sibling;

    if ((door->flags2 & PROPFLAG2_DOOR_AIRLOCK) && state == DOORSTATE_OPENING)
    {
        linkedState = DOORSTATE_CLOSING;

        if (door->openstate == DOORSTATE_STATIONARY)
        {
            state = DOORSTATE_WAITING;
        }
    }

    doorSetState(door, state);

    for (sibling = door->linkedDoor; sibling != NULL && sibling != door; sibling = sibling->linkedDoor)
    {
        doorSetState(sibling, linkedState);
    }
}

/**
 * Someone used the door: it turns round if it is moving, and otherwise opens if it is less
 * than half open and closes if not. Switches linked to the door use it too.
 */
void doorToggle(PropRecord *prop)
{
    DoorRecord *door = prop->door;

    if (door->openstate == DOORSTATE_OPENING || door->openstate == DOORSTATE_WAITING)
    {
        doorMove(door, DOORSTATE_CLOSING);
    }
    else if (door->openstate == DOORSTATE_CLOSING)
    {
        doorMove(door, DOORSTATE_OPENING);
    }
    else if (door->openstate == DOORSTATE_STATIONARY)
    {
        if (door->openPosition > 0.5f)
        {
            doorMove(door, DOORSTATE_CLOSING);
        }
        else
        {
            doorMove(door, DOORSTATE_OPENING);
        }
    }

    door->runtime_bitflags |= RUNTIMEBITFLAG_ACTIVATED;
    coopNoteActivator();
    door->flags2 &= ~PROPFLAG2_DOOR_TRIED_LOCKED;
    switchActivateLinkedDoors(prop);
}

/** Opens the door all the way at once, silently (the AI command door_open_instant). */
void doorOpenInstantly(DoorRecord *door)
{
    door->speed = 0;
    door->openPosition = door->maxFrac;
    door->openedTime = g_GlobalTimer;
    door->openstate = DOORSTATE_STATIONARY;
    doorUpdateCollision(door);
    doorOpenPortal(door);
    doorStopSounds(door);
}

bool doorIsClosed(DoorRecord *door)
{
    return (door->openstate == DOORSTATE_STATIONARY || door->openstate == DOORSTATE_WAITING) && door->openPosition <= 0.0f;
}

/** Moves the door a tick towards open or shut. Returns whether it is moving. */
static bool doorApplySpeed(DoorRecord *door)
{
    f32 target;

    if (door->openstate == DOORSTATE_OPENING)
    {
        target = door->maxFrac;
    }
    else if (door->openstate == DOORSTATE_CLOSING)
    {
        target = 0.0f;
    }
    else
    {
        return FALSE;
    }

    chrobjApplySpeed(&door->openPosition, target, &door->speed, door->accel, door->decel, door->maxSpeed);

    if (door->openPosition >= door->maxFrac)
    {
        door->openPosition = door->maxFrac;
    }
    else if (door->openPosition <= 0.0f)
    {
        door->openPosition = 0.0f;
    }

    return TRUE;
}

/**
 * Moves the door and its linked doors a tick. If any of them is blocked (by a guard, the
 * player or an object), they all go back to where they were and try again next tick.
 */
static void doorsTickMovement(DoorRecord *door)
{
    DoorRecord *sibling;
    bool moved = FALSE;
    bool clear = TRUE;

    FOR_EACH_LINKED_DOOR(sibling, door)
    {
        sibling->positionBeforeMove = sibling->openPosition;

        if (doorApplySpeed(sibling))
        {
            moved = TRUE;
        }
    }

    if (moved)
    {
        FOR_EACH_LINKED_DOOR(sibling, door)
        {
            doorUpdateCollision(sibling);
            clear = vehicleFindBlockingProp(sibling->prop);

            if (!clear)
            {
                break;
            }
        }
    }

    FOR_EACH_LINKED_DOOR(sibling, door)
    {
        if (moved)
        {
            if (clear)
            {
                if (sibling->openstate == DOORSTATE_OPENING)
                {
                    if (sibling->openPosition >= sibling->maxFrac)
                    {
                        sibling->openstate = DOORSTATE_STATIONARY;
                        sibling->speed = 0.0f;
                        sibling->openedTime = g_GlobalTimer;
                        doorFinishOpening(sibling);
                    }
                }
                else if (sibling->openstate == DOORSTATE_CLOSING && sibling->openPosition <= 0.0f)
                {
                    sibling->openstate = DOORSTATE_STATIONARY;
                    sibling->speed = 0.0f;
                    sibling->openedTime = 0;
                    doorFinishClosing(sibling);
                }

                objGetShadeColour(sibling->prop, &sibling->nextcol);
            }
            else
            {
                sibling->speed = 0.0f;
                sibling->openPosition = sibling->positionBeforeMove;
                doorUpdateCollision(sibling);
            }

            doorClipVertices(sibling);
        }
        else if (sibling->doorFlags & DOORFLAG_CLIP_TO_BBOX)
        {
            doorKeepClippedVertices(sibling);
        }

        sibling->lastUpdateTime = g_GlobalTimer;
    }
}

/** The door's part of objTick: closing itself, the airlock wait, and moving. */
void doorTick(PropRecord *prop)
{
    DoorRecord *door = prop->door;
#if defined(VERSION_EU)
    s32 autoClose = ((s32)door->autoCloseFrames * 50) / 60;
#else
    s32 autoClose = door->autoCloseFrames;
#endif

    /* Closes itself once it has been open long enough, unless it was set to stay open */
    if ((s32)door->openedTime > 0
        && (s32)door->openedTime < (s32)g_GlobalTimer - autoClose
        && door->openstate == DOORSTATE_STATIONARY
        && !(door->flags & PROPFLAG_DOOR_KEEPOPEN))
    {
        doorMove(door, DOORSTATE_CLOSING);
    }

    /* An airlock door opens once all its linked doors have shut */
    if (door->openstate == DOORSTATE_WAITING)
    {
        bool allClosed = TRUE;

        for (DoorRecord *sibling = door->linkedDoor; sibling != NULL && sibling != door; sibling = sibling->linkedDoor)
        {
            if (sibling->openstate != DOORSTATE_STATIONARY || sibling->openPosition > 0.0f)
            {
                allClosed = FALSE;
            }
        }

        if (allClosed)
        {
            doorSetState(door, DOORSTATE_OPENING);
        }
    }

    /* A grate falls as soon as its padlock is shot off */
    if (door->doorType == DOORTYPE_FALLAWAY && doorIsClosed(door) && doorIsPadlockFree(door))
    {
        doorToggle(prop);
    }

    if (door->lastUpdateTime < g_GlobalTimer || g_ClockTimer == 0)
    {
        doorsTickMovement(door);
    }
}


/*
 * Using a door
 */

/** The angle from where the player looks (yaw) to a point seen from eye, from -pi to pi. */
static f32 angleFromView(f32 x, f32 z, const coord3d *eye, f32 yaw)
{
    f32 angle = mathAtan2(x - eye->x, z - eye->z);
    f32 diff = angle - yaw;

    if (angle < yaw)
    {
        diff += M_TAU_F;
    }

    if (diff > M_PI_F)
    {
        diff -= M_TAU_F;
    }

    return diff;
}

/**
 * The angles from where the player looks to the door's two edges, smallest first. The
 * door's edges are along the pad's up axis, or its side axis for altAxis. They are measured
 * from a little in front of the player. A swinging door's far edge is where it has swung to.
 */
static void doorGetAnglesToPlayer(DoorRecord *door, f32 *minangle, f32 *maxangle, bool altAxis)
{
    BoundPadRecord *pad = &g_CurrentSetup.boundpads[door->pad];
    PropRecord *playerprop = currentPlayerGetProp();
    f32 yaw = bondviewGetPlayerYawRadians();
    coord3d eye;
    coord3d axis;
    f32 edgeMin;
    f32 edgeMax;
    f32 nearX;
    f32 nearZ;
    f32 angle1;
    f32 angle2;

    eye.x = ((g_CurrentPlayer->bond.theta_transform.x * 30.0f) * 0.75f) + playerprop->pos.x;
    eye.y = playerprop->pos.y;
    eye.z = ((g_CurrentPlayer->bond.theta_transform.z * 30.0f) * 0.75f) + playerprop->pos.z;

    if (altAxis)
    {
        edgeMin = pad->bbox.xmin;
        edgeMax = pad->bbox.xmax;
        padGetSide(pad, &axis);
    }
    else
    {
        edgeMin = pad->bbox.ymin;
        edgeMax = pad->bbox.ymax;
        axis = pad->up;
    }

    nearX = pad->pos.x + (axis.x * edgeMin);
    nearZ = pad->pos.z + (axis.z * edgeMin);
    angle1 = angleFromView(nearX, nearZ, &eye, yaw);

    if (door->doorType == DOORTYPE_SWINGING)
    {
        f32 swing = degreesToRadians(door->openPosition);
        f32 width = edgeMax - edgeMin;
        f32 cosine;
        f32 sine;

        if (door->flags & PROPFLAG_DOOR_OPENTOFRONT)
        {
            swing = M_TAU_F - swing;
        }

        cosine = cosf(swing);
        sine = sinf(swing);

        angle2 = angleFromView(nearX + (width * ((axis.x * cosine) + (axis.z * sine))),
                               nearZ + (width * ((-axis.x * sine) + (axis.z * cosine))), &eye, yaw);
    }
    else
    {
        angle2 = angleFromView(pad->pos.x + (axis.x * edgeMax), pad->pos.z + (axis.z * edgeMax), &eye, yaw);
    }

    if (angle1 < angle2)
    {
        *minangle = angle1;
        *maxangle = angle2;
    }
    else
    {
        *minangle = angle2;
        *maxangle = angle1;
    }
}

/**
 * Whether the player is looking at the door: both its edges within 20 degrees of straight
 * ahead, or the door (with its linked doors) across the middle of the view. If so it becomes
 * g_InteractProp. Returns FALSE when it has, so the search can stop.
 */
static bool doorTestInteractAngle(DoorRecord *door, bool altAxis)
{
    f32 minangle;
    f32 maxangle;

    if (g_InteractProp != NULL)
    {
        return TRUE;
    }

    doorGetAnglesToPlayer(door, &minangle, &maxangle, altAxis);

    if (minangle >= -DOOR_INTERACT_ANGLE && minangle <= DOOR_INTERACT_ANGLE
        && maxangle >= -DOOR_INTERACT_ANGLE && maxangle <= DOOR_INTERACT_ANGLE)
    {
        g_InteractProp = door->prop;
        return FALSE;
    }

    /* Widen to the linked doors, until the doors cross the middle of the view */
    for (DoorRecord *sibling = door->linkedDoor;
         sibling != NULL && sibling != door && (minangle >= 0.0f || maxangle < 0.0f);
         sibling = sibling->linkedDoor)
    {
        f32 siblingmin;
        f32 siblingmax;

        doorGetAnglesToPlayer(sibling, &siblingmin, &siblingmax, altAxis);

        if (minangle > 0.0f && siblingmin < minangle)
        {
            minangle = siblingmin;
        }

        if (maxangle < 0.0f && maxangle < siblingmax)
        {
            maxangle = siblingmax;
        }
    }

    if ((maxangle - minangle) < M_PI_F && minangle < 0.0f && maxangle > 0.0f)
    {
        g_InteractProp = door->prop;
        return FALSE;
    }

    return TRUE;
}

/**
 * Whether the current player can use the door: near it and looking at it. If so it becomes
 * g_InteractProp. Returns FALSE when it has, so the search can stop.
 */
bool doorTestForInteract(PropRecord *prop)
{
    DoorRecord *door = prop->door;
    PropRecord *playerprop;
    bool inReach = FALSE;
    bool checkmore = TRUE;
    f32 xdiff;
    f32 ydiff;
    f32 zdiff;

    if ((door->flags & PROPFLAG_CANNOT_ACTIVATE) || !(door->maxFrac > 0) || !(prop->flags & PROPFLAG_ONSCREEN))
    {
        return TRUE;
    }

    playerprop = currentPlayerGetProp();
    xdiff = door->runtime_pos.x - playerprop->pos.x;
    ydiff = door->runtime_pos.y - playerprop->pos.y;
    zdiff = door->runtime_pos.z - playerprop->pos.z;

    if (xdiff * xdiff + zdiff * zdiff < DOOR_REACH_SQ && ydiff < DOOR_REACH_Y && ydiff > -DOOR_REACH_Y)
    {
        inReach = TRUE;
    }
    else
    {
        /* A wide door: near any part of it, in the same room */
        s32 doorRooms[8];
        s32 playerRooms[8];

        propGetRoomIds(prop, doorRooms);
        propGetRoomIds(playerprop, playerRooms);

        if (roomListsOverlap(doorRooms, playerRooms)
            && padBoundsContainPoint(&playerprop->pos, DOOR_REACH_PAD_MARGIN, &g_CurrentSetup.boundpads[door->pad]))
        {
            inReach = TRUE;
        }
    }

    if (inReach)
    {
        checkmore = doorTestInteractAngle(door, FALSE);

        if (checkmore && (door->flags2 & PROPFLAG2_DOOR_ALTCOORDSYSTEM))
        {
            checkmore = doorTestInteractAngle(door, TRUE);
        }
    }

    return checkmore;
}

/** Whether the prop is in front of the door (on its side axis side). Exactly in line counts as in front. */
static bool doorIsInFront(DoorRecord *door, PropRecord *prop)
{
    BoundPadRecord *pad = &g_CurrentSetup.boundpads[door->pad];
    coord3d side;
    f32 dist;

    padGetSide(pad, &side);

    dist = ((prop->pos.x - pad->pos.x) * side.x) + ((prop->pos.y - pad->pos.y) * side.y) + ((prop->pos.z - pad->pos.z) * side.z);

    if (door->doorFlags & DOORFLAG_FLIP)
    {
        dist = -dist;
    }

    return !(dist < 0.0f);
}

/** A two-way swinging door that is shut opens away from whoever opens it (and its linked doors with it). */
void doorChooseSwingDirection(PropRecord *chrprop, DoorRecord *door)
{
    if ((door->flags & PROPFLAG_DOOR_TWOWAY) && door->openstate == DOORSTATE_STATIONARY && door->openPosition == 0.0f)
    {
        bool infront = doorIsInFront(door, chrprop);
        bool toFront = (door->doorFlags & DOORFLAG_FLIP) ? infront : !infront;
        DoorRecord *sibling;

        if (toFront != ((door->flags & PROPFLAG_DOOR_OPENTOFRONT) != 0))
        {
            FOR_EACH_LINKED_DOOR(sibling, door)
            {
                sibling->flags ^= PROPFLAG_DOOR_OPENTOFRONT;
            }
        }
    }
}

/** The current player uses the door: it opens or closes, or says it is locked. */
TICKOP doorInteract(PropRecord *prop)
{
    DoorRecord *door = prop->door;
    PropRecord *playerprop = currentPlayerGetProp();
    bool canOpen = FALSE;

    if (door->keyflags == 0 || invCheckHasKeyFlags(door->keyflags))
    {
        canOpen = TRUE;
    }
    else if (doorIsInFront(door, playerprop))
    {
        canOpen = (door->flags2 & PROPFLAG2_DOOR_UNLOCKED_FROM_FRONT) && !(door->flags2 & PROPFLAG2_DOOR_UNLOCKED_FROM_BACK);
    }
    else
    {
        canOpen = !(door->flags2 & PROPFLAG2_DOOR_UNLOCKED_FROM_FRONT) && (door->flags2 & PROPFLAG2_DOOR_UNLOCKED_FROM_BACK);
    }

    if (!doorIsPadlockFree(door))
    {
        canOpen = FALSE;
    }

    /* Counter-op: a door whose opening is Bond's progress (Depot's train) stays shut for the
     * guard player, showing as locked. */
    if (counterOpIsGuardPlayer(playerGetCurrentNum()) && counterOpDoorIsScripted(door))
    {
        canOpen = FALSE;
    }

    if (canOpen)
    {
        doorChooseSwingDirection(playerprop, door);
        doorToggle(prop);
    }
    else if (door->openstate == DOORSTATE_STATIONARY && door->openPosition < 0.5f)
    {
        if (!(door->flags2 & PROPFLAG2_DOOR_NO_LOCKED_TEXT))
        {
            textoverride *txt = invGetOverrideForObj((ObjectRecord *)door);

            if (txt != NULL && txt->pickuptext != 0)
            {
                HUDMESSAGEBOTTOM(langGet((s32)txt->pickuptext));
            }
            else
            {
                HUDMESSAGEBOTTOM(langGet(getStringID(LPROPOBJ, PROPOBJ_STR_40_THISDOORISLOCKED)));
            }
        }

        /* Counter-op: the guard player trying a locked door isn't news to the level scripts. */
        if (!counterOpIsGuardPlayer(playerGetCurrentNum()))
        {
            door->runtime_bitflags |= RUNTIMEBITFLAG_ACTIVATED;
            coopNoteActivator();
        }

        door->flags2 |= PROPFLAG2_DOOR_TRIED_LOCKED;
    }

    return TICKOP_NONE;
}


/*
 * Drawing
 */

/**
 * The door model's matrices for drawing, in screen space: the door itself, then for the
 * Caverns doors their moving parts. The eye door's two lids turn apart; each of the iris
 * door's six blades tilts out once the door is 30% open, as its tip turns. The iris plays a
 * sound as its blades start and finish (playSounds: once in co-op).
 */
void doorGetModelMatrices(DoorRecord *door, Model *model, Mtxf *mtxs, f32 prevOpenPosition, bool playSounds)
{
    doorGetMtx(door, mtxs);
    mtx4MultAffineInPlace(camGetWorldToScreenMtxf(), mtxs);

    if (model->obj->Skeleton == &skeleton_eyelid_door)
    {
        f32 lidAngle = M_TAU_F - degreesToRadians(door->openPosition);

        mtx4LoadXRotation(lidAngle, &mtxs[1]);
        mtx4SetTranslation(&model->obj->Switches[1]->Data->Group.Origin, &mtxs[1]);
        mtx4MultInPlace(mtxs, &mtxs[1]);

        mtx4LoadXRotation(M_TAU_F - lidAngle, &mtxs[2]);
        mtx4SetTranslation(&model->obj->Switches[2]->Data->Group.Origin, &mtxs[2]);
        mtx4MultInPlace(mtxs, &mtxs[2]);
    }
    else if (model->obj->Skeleton == &skeleton_iris_door)
    {
        f32 bladeAngle = 0.0f;
        f32 tipAngle = degreesToRadians(door->openPosition);
        f32 bladeStart = door->maxFrac * 0.3f;

        if (bladeStart < door->openPosition)
        {
            bladeAngle = degreesToRadians((door->maxFrac * (door->openPosition - bladeStart)) / (door->maxFrac - bladeStart));

            if (playSounds && prevOpenPosition <= bladeStart)
            {
                sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_OPEN_SFX, NULL), &door->prop->pos);
            }
        }
        else if (playSounds && bladeStart < prevOpenPosition)
        {
            sfxSetVolumeAt(sndPlaySfx(g_musicSfxBufferPtr, METAL_SLIDE_CLOSE_SFX, NULL), &door->prop->pos);
        }

        /* Each blade is a pivot node with its tip as a child node */
        for (s32 i = 0; i < 6; i++)
        {
            Mtxf *blademtx = &mtxs[i * 2 + 1];
            Mtxf *tipmtx = &mtxs[i * 2 + 2];

            mtx4LoadZRotation(bladeAngle, blademtx);
            mtx4SetTranslation(&model->obj->Switches[i * 2 + 1]->Data->Group.Origin, blademtx);
            mtx4MultInPlace(mtxs, blademtx);
            mtx4LoadZRotation(tipAngle, tipmtx);
            mtx4SetTranslation(&model->obj->Switches[i * 2 + 2]->Data->Group.Origin, tipmtx);
            mtx4MultInPlace(blademtx, tipmtx);
        }
    }
}

/** A glass door's pane breaks into shards (after three hits). */
void doorShatterGlass(DoorRecord *door)
{
    Model *model = door->model;
    const struct ModelRoData_BoundingBoxRecord *pane = &model->obj->Switches[2]->Data->BoundingBox;
    Mtxf mtx;

    doorGetMtx(door, &mtx);
    glassCreateShards((coord3d *)mtx.m[3], (coord3d *)mtx.m[0], (coord3d *)mtx.m[1], (coord3d *)mtx.m[2],
                      pane->Bounds.xmin, pane->Bounds.xmax, pane->Bounds.ymin, pane->Bounds.ymax);

    explosionClearBulletImpactRoomByFlag(door->prop, 1);

    modelGetNodeRwData(model, model->obj->Switches[1])->Switch.visible = FALSE;
}
