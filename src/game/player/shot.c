/**
 * Firing shots into the world: tracing the bullet through rooms and
 * props, what it hits (shotFireHands, shotFireBullet), and the hit sounds.
 */
#include <bondaicommands.h>
#include <music.h>
#include "assets/obseg/text/LgunE.h"
#include "game/audio/sfxvolume.h"
#include "game/chr/chraction.h"
#include "game/hud/sparks.h"
#include "game/level/lv.h"
#include "game/menu/watchmenu.h"
#include "game/mission/objective.h"
#include "game/nav/stan.h"
#include "game/player/bondview.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/prop/explosion.h"
#include "game/prop/propobj.h"
#include "game/world/bg.h"
#include "game/world/lightfixture.h"
#include "game/world/surface.h"
#include "random.h"
#include "game/player/shot.h"

// Sounds for bullets hitting things, picked by surface
static u16 ricochet_sounds_small[20] = {
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_8_AFDM_A_SFX, RICO_8_AFDM_B_SFX, RICO_8_AFDM_C_SFX, RICO_8_AFDM_D_SFX,
    RICO_5_A_SFX,      RICO_5_B_SFX,      RICO_5_C_SFX,      RICO_5_D_SFX,
    RICO_6_HBBA_A_SFX, RICO_6_HBBA_B_SFX, RICO_6_HBBA_C_SFX, RICO_6_HBBA_D_SFX
};

static u16 punch_sounds[3] = {
    PUNCH1_SFX,
    PUNCH2_SFX,
    PUNCH3_SFX
};

static u16 bullet_flesh_sounds[2] = {
    HIT_BULLET_FLESH_SFX,
    HIT_BULLET_FLESH_SFX
};

static u16 laser_ricochet_sounds[2] = {
    RICO_LASER2_SFX,
    RICO_LASER3_SFX
};

static u16 ricochet_sounds_large[36] = {
	RICO_12_GBU_A_SFX, RICO_12_GBU_B_SFX, RICO_12_GBU_C_SFX, RICO_12_GBU_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_6_TAJ_A_SFX,  RICO_6_TAJ_B_SFX,  RICO_6_TAJ_C_SFX,  RICO_6_TAJ_D_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_4_A_SFX,      RICO_4_B_SFX,      RICO_4_B_SFX,      RICO_4_C_SFX,
    RICO_5_A_SFX,      RICO_5_B_SFX,      RICO_5_C_SFX,      RICO_5_D_SFX,
    RICO_6_HBBA_A_SFX, RICO_6_HBBA_B_SFX, RICO_6_HBBA_C_SFX, RICO_6_HBBA_D_SFX
};


/**
 * Returns a free hit sound slot, or NULL if all are playing.
 */
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


/**
 * Plays the sound of a shot hitting a prop or a guard.
 */
void shotPlayHitPropSound(enum ITEM_IDS item, PropRecord* prop, s32 texture_index)
{
    s32 volume;
    u32 rnd1;
    u32 rnd2;
    ALSoundState **sound_state;
    u32 sfx_index;

    volume = sfxVolumeAt(&prop->pos);

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
            sfxSetVolumeAt(*sound_state, &prop->pos);
        }
    }
}


/**
 * Plays the sound of a shot breaking glass at pos.
 */
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
            sfxSetVolumeAt(*sound, pos);
        }
    }
}


/**
 * Plays the ricochet sound of a shot hitting the world, chosen by the
 * weapon and the surface's texture.
 */
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
            sfxSetVolumeAt(*sound_state, pos);
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
                sfxSetVolumeAt(*sound_state, pos);
            }
        }
    }
}


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
        end = roomptr + numrooms;

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

                        if ((bestroom <= 0)
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

    if (stanWalkLine(&fromtile, playerprop->pos.x, playerprop->pos.z, shotdata.gunpos.x, shotdata.gunpos.z))
    {
        distscale = bgGetLevelScale() * bgGetLevelVisibilityScale();
        playerpos = playerGetPos();


        stanResetHits();

        if (!stanWalkLine(&fromtile, shotdata.gunpos.x, shotdata.gunpos.z, dest.x, dest.z))
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
        startroom = fromtile->room;

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
                bestroom = shotFindBgHitInConnectedRooms(currentPlayerGetProp()->stan->room, playerpos, &stanhit, &hitdir, &scaleddir, visited, &bghit);
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
                    impactCreate(&visiblehitpos, &bghit.normal, surface->impacts[pad], bestroom, 0, -1, 0);
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
                    explosionCreate(NULL, finalpos, hittile, EXPLOSION_DEF_BULLETPUFF, 0, playerGetCurrentNum(), rooms, 0);
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


/**
 * Hits whatever is in reach in front of the player with an unarmed or
 * melee attack (fists or knife).
 */
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


/**
 * Fires one hand's weapon if it fired this frame: bullets, a punch, a tank
 * shell, a photo, or setting off remote mines.
 */
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


/**
 * Fires both hands for the current player.
 */
void shotFireHands(void)
{
    shotFireHand(GUNRIGHT);
    shotFireHand(GUNLEFT);
}
