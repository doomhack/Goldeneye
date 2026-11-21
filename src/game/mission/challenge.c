/**
 * Challenge mode: see challenge.h.
 *
 * The challenge for a stage and difficulty comes from s_Challenges. It replaces
 * the mission objectives: their status messages are switched off, the debrief
 * shows the challenge instead, and success is the challenge's result. The
 * level's scripts still run, so doors, alarms and guards behave as normal.
 *
 * The time limit counts mission time, so it stops while the watch is up and
 * while a script pauses the mission clock (cutscenes).
 */
#include <ultra64.h>
#include <bondtypes.h>
#include <bondaicommands.h>
#include <boss.h>
#include "challenge.h"
#include "objective.h"
#include "mission.h"
#include "game/level/gamemode.h"
#include "game/level/lv.h"
#include "game/player/player.h"
#include "game/player/playerupdate.h"
#include "game/prop/setup.h"
#include "game/nav/stan.h"
#include "game/hud/ammohud.h"
#include "game/text/text.h"
#include "game/prop/prop.h"
#include "game/prop/objdamage.h"
#include "game/player/inventory.h"
#include "game/player/gun.h"
#include "game/player/ammo.h"
#include "game/player/playerlife.h"
#include "game/player/tank.h"
#include "game/chr/chr.h"
#include "game/chr/chrai.h"
#include "game/chr/chrscript.h"
#include "game/chr/chrsight.h"
#include "game/prop/carried.h"
#include "game/math/trig.h"
#include <random.h>
#include "game/player/playerintro.h"
#include "fr.h"
#include "str.h"

// The game clock runs at 60 ticks a second in every region
#define TICKS_PER_SEC 60

// World units per metre for the challenge distances (far kills, Survive's
// area and spawn ring, photo range). Zak found 100 made "100 m" look far too
// far on Dam; 50 was too short, so a metre is 75 units.
#define UNITS_PER_METRE 75.0f

// How long the result stays on screen before the level ends
#define RESULT_TICKS (2 * TICKS_PER_SEC)

// Table shorthand
#define AT(level, diff, secs, title) \
    .stage = LEVELID_##level, .difficulty = DIFFICULTY_##diff, .timelimit = (secs), .name = (title)
#define COUNT(kind, n) .count = CHALLENGE_##kind, .target = (n), .pads = { -1, -1 }, .doorpad = -1
#define DESTROY(kind, n) COUNT(DESTROY, n), .targets = CHALLENGE_TARGETS_##kind
#define REACH(pad1, pad2) .count = CHALLENGE_REACH, .pads = { (pad1), (pad2) }, .doorpad = -1
#define REACH_DOOR(pad) .count = CHALLENGE_REACH, .pads = { -1, -1 }, .doorpad = (pad)
#define SURVIVE(startpad, metres) \
    .count = CHALLENGE_SURVIVE, .pads = { (startpad), -1 }, .doorpad = -1, .param = (metres)
#define ITEMS(right, left) .items = { (right), (left), ITEM_UNARMED }
#define TWO(item) ITEMS(item, item)
// Placeholder until the level gets real challenges: kill one enemy in a minute
#define PLACEHOLDER(level, diff) { AT(level, diff, 60, "PLACEHOLDER"), COUNT(KILLS, 1) }

/**
 * Zak's list (2026-11-14). Reach pads are the rooms the level's exit script
 * looks for; with CF_EXIT the script ending the level also passes, for exits
 * that are a cutscene rather than a room (Dam's bungee jump).
 */
static const ChallengeDef s_Challenges[] = {
    // Dam
    { AT(DAM, AGENT, 60, "SHARPSHOOTER"), COUNT(FAR_KILLS, 3), .param = 100, ITEMS(ITEM_SNIPERRIFLE, ITEM_UNARMED) },
    { AT(DAM, SECRET, 60, "DESTRUCTION"), DESTROY(GLASS, 12), ITEMS(ITEM_REMOTEMINE, ITEM_UNARMED), .flags = CF_KEEP_GUN },
    { AT(DAM, 00, 600, "GHOST"), REACH(0x14a, -1), .goal = "Reach the bungee jump", TWO(ITEM_WPPKSIL), .flags = CF_EXIT | CF_FAIL_SEEN | CF_FAIL_ALARM },

    // Facility. Bound pad 93 is the remotely locked security door (door 63)
    // on the lower floor.
    { AT(FACILITY, AGENT, 90, "HAT TRICK"), COUNT(HATS, 3) },
    { AT(FACILITY, SECRET, 120, "MARKSMAN"), REACH(0x87, 0x135), .goal = "Reach the exit", .flags = CF_EXIT | CF_FAIL_MISS },
    { AT(FACILITY, 00, 60, "UNTOUCHABLE"), REACH_DOOR(10093), .goal = "Get past the security door", .flags = CF_FAIL_DAMAGE },

    // Runway (guards respawn)
    { AT(RUNWAY, AGENT, 180, "VEHICLE"), COUNT(TANK_KILLS, 25) },
    { AT(RUNWAY, SECRET, 60, "DESTRUCTION"), DESTROY(AUTOGUNS, 0) },
    { AT(RUNWAY, 00, 180, "SURVIVE"), SURVIVE(-1, 15), .items = { ITEM_UZI, ITEM_UZI, ITEM_PROXIMITYMINE } },

    // Surface 1
    { AT(SURFACE, AGENT, 60, "SHARPSHOOTER"), COUNT(FAR_KILLS, 3), .param = 100 },
    { AT(SURFACE, SECRET, 60, "DESTRUCTION"), DESTROY(GLASS, 16), ITEMS(ITEM_GRENADE, ITEM_UNARMED), .flags = CF_KEEP_GUN },
    { AT(SURFACE, 00, 120, "UNTOUCHABLE"), REACH(0x2e, -1), .goal = "Reach the bunker door", .flags = CF_EXIT | CF_FAIL_DAMAGE },

    // Bunker 1
    { AT(BUNKER1, AGENT, 30, "DESTRUCTION"), DESTROY(MODELS, 0), .goal = "Destroy the mainframes", .models = { PROP_MAINFRAME1, PROP_MAINFRAME2 } },
    { AT(BUNKER1, SECRET, 60, "HAT TRICK"), COUNT(HATS, 5) },
    { AT(BUNKER1, 00, 150, "GHOST"), REACH(0x33, -1), .goal = "Escape the bunker", .flags = CF_EXIT | CF_FAIL_SEEN | CF_FAIL_ALARM },

    // Silo
    { AT(SILO, AGENT, 30, "CHAIN BLAST"), COUNT(CHAIN, 5), TWO(ITEM_ROCKETLAUNCH), .flags = CF_REPLACE },
    { AT(SILO, SECRET, 30, "RAMPAGE"), COUNT(KILLS, 25), TWO(ITEM_GRENADELAUNCH), .flags = CF_REPLACE },
    { AT(SILO, 00, 300, "ARSENAL"), REACH(0xe8, -1), .goal = "Reach the exit lift", TWO(ITEM_GOLDENGUN), .flags = CF_EXIT | CF_REPLACE | CF_NO_PICKUPS },

    // Frigate
    { AT(FRIGATE, AGENT, 60, "RAMPAGE"), COUNT(KILLS, 30), TWO(ITEM_MP5KSIL) },
    { AT(FRIGATE, SECRET, 45, "DESTRUCTION"), DESTROY(MODELS, 0), .goal = "Blow up both bombs", .models = { PROP_CHRPLASTIQUE } },
    { AT(FRIGATE, 00, 60, "CHAIN BLAST"), COUNT(CHAIN, 5) },

    // Surface 2
    { AT(SURFACE2, AGENT, 180, "DESTRUCTION"), DESTROY(CCTV, 0), TWO(ITEM_WPPK) },
    { AT(SURFACE2, SECRET, 60, "DESTRUCTION"), DESTROY(MODELS, 0), .goal = "Destroy both helicopters", .models = { PROP_MILCOPTER }, TWO(ITEM_SILVERWPPK), .flags = CF_MAKE_MORTAL },
    { AT(SURFACE2, 00, 300, "DESTRUCTION"), DESTROY(GLASS, 0), TWO(ITEM_GOLDWPPK) },

    // Bunker 2
    { AT(BUNKER2, AGENT, 180, "DESTRUCTION"), DESTROY(MONITORS, 0) },
    { AT(BUNKER2, SECRET, 60, "ARSENAL"), REACH(0x64, -1), .goal = "Escape the bunker", TWO(ITEM_THROWKNIFE), .flags = CF_EXIT | CF_REPLACE | CF_NO_PICKUPS },
    { AT(BUNKER2, 00, 180, "GHOST"), REACH(0x64, -1), .goal = "Escape the bunker", TWO(ITEM_THROWKNIFE), .flags = CF_EXIT | CF_FAIL_SEEN | CF_FAIL_ALARM },

    // Statue. Pad 59 is the statue where Bond meets Trevelyan (Statue ai_36),
    // pad 8 is where Valentin stands.
    { AT(STATUE, AGENT, 120, "SURVIVE"), SURVIVE(59, 15), TWO(ITEM_AUTOSHOT), .flags = CF_START_PAD },
    PLACEHOLDER(STATUE, SECRET),
    { AT(STATUE, 00, 60, "UNTOUCHABLE"), REACH(8, -1), .goal = "Reach Valentin", .flags = CF_FAIL_DAMAGE },

    // Archives. It has no wooden crates, so these are the cardboard boxes (124
    // of them, too many for all).
    { AT(ARCHIVES, AGENT, 180, "DESTRUCTION"), DESTROY(MODELS, 30), .goal = "Destroy %d cardboard boxes", .models = { PROP_CARD_BOX1 }, ITEMS(ITEM_GRENADE, ITEM_UNARMED) },
    { AT(ARCHIVES, SECRET, 60, "HAT TRICK"), COUNT(HATS, 5) },
    { AT(ARCHIVES, 00, 120, "PACIFIST"), REACH(0xd0, -1), .goal = "Escape the archives", .flags = CF_EXIT | CF_FAIL_HARM },

    // Streets. Chr 31 is Valentin.
    { AT(STREETS, AGENT, 300, "VEHICLE"), DESTROY(VEHICLES, 0) },
    { AT(STREETS, SECRET, 300, "PACIFIST"), REACH(0xf8, -1), .goal = "Reach the exit", .flags = CF_EXIT | CF_FAIL_HARM },
    { AT(STREETS, 00, 120, "ASSASSIN"), COUNT(KILL_CHR, 1), .goal = "Kill Valentin", .param = 31, .flags = CF_FAIL_ALARM },

    // Depot (guards respawn)
    { AT(DEPOT, AGENT, 120, "RAMPAGE"), COUNT(KILLS, 25), TWO(ITEM_FNP90) },
    { AT(DEPOT, SECRET, 60, "MARKSMAN"), COUNT(HEAD_KILLS, 10), TWO(ITEM_WPPK), .flags = CF_FAIL_BODYKILL },
    PLACEHOLDER(DEPOT, 00),

    PLACEHOLDER(TRAIN, AGENT), PLACEHOLDER(TRAIN, SECRET), PLACEHOLDER(TRAIN, 00),
    PLACEHOLDER(JUNGLE, AGENT), PLACEHOLDER(JUNGLE, SECRET), PLACEHOLDER(JUNGLE, 00),
    PLACEHOLDER(CONTROL, AGENT), PLACEHOLDER(CONTROL, SECRET), PLACEHOLDER(CONTROL, 00),
    PLACEHOLDER(CAVERNS, AGENT), PLACEHOLDER(CAVERNS, SECRET), PLACEHOLDER(CAVERNS, 00),

    // Cradle. Chr 0 is Trevelyan.
    { AT(CRADLE, AGENT, 300, "ARSENAL"), COUNT(KILL_CHR, 1), .goal = "Kill Trevelyan", .param = 0, ITEMS(ITEM_REMOTEMINE, ITEM_UNARMED), .flags = CF_REPLACE | CF_NO_PICKUPS },
    { AT(CRADLE, SECRET, 60, "PHOTOGRAPH"), COUNT(PHOTO, 1), .goal = "Photograph Trevelyan", .param = 0, ITEMS(ITEM_CAMERA, ITEM_UNARMED), .flags = CF_KEEP_GUN },
    PLACEHOLDER(CRADLE, 00),

    PLACEHOLDER(AZTEC, AGENT), PLACEHOLDER(AZTEC, SECRET), PLACEHOLDER(AZTEC, 00),
    PLACEHOLDER(EGYPT, AGENT), PLACEHOLDER(EGYPT, SECRET), PLACEHOLDER(EGYPT, 00),
};

// Explosion kills this close together (ticks) are one chain
#define CHAIN_TICKS TICKS_PER_SEC

const ChallengeDef *g_Challenge = NULL;
bool g_ChallengeMenu = FALSE;

static ChallengeState s_State;
static s32 s_StartTime;         // mission time (ticks) when the player got control, -1 before
static s32 s_ResultTicks;       // counts down the result message once decided
static bool s_LevelEnding;      // the level end has been requested
static s32 s_Count;              // progress towards the target
static s32 s_Target;             // how many to count (set when the clock starts)
static s32 s_Chain;              // explosion kills in the current chain
static s32 s_ChainTime;          // g_GlobalTimer of the last one

// Survive: the spawner
static coord3d s_SurviveCentre;  // where the player was when the clock started
static s32 s_SpawnTicks;         // until the next spawn
static s32 s_Spawned;            // guards spawned so far
static s32 s_PadCount;
static s8 s_SpawnBody;           // what spawned guards look like and carry,
static s16 s_SpawnGunModel;      // copied from one of the level's guards
static s16 s_SpawnGun;
static bool s_ShotActive;       // a player shot is being resolved
static bool s_ShotHitEnemy;     // and it hit an enemy
static f32 s_DoorSide;          // which side of the goal door the player started on (+1/-1)

// What the debrief shows, kept after the stage unloads
static const ChallengeDef *s_Played;
static bool s_Passed;
static s32 s_PassSecs;          // how long it took, rounded up

const ChallengeDef *challengeFind(LEVELID stage, DIFFICULTY difficulty)
{
    s32 i;

    for (i = 0; i < (s32)ARRAYCOUNT(s_Challenges); i++)
    {
        if (s_Challenges[i].stage == stage && s_Challenges[i].difficulty == difficulty)
        {
            return &s_Challenges[i];
        }
    }

    return NULL;
}

s32 challengeGetHighestDifficulty(LEVELID stage)
{
    s32 highest = -1;
    s32 i;

    for (i = 0; i < (s32)ARRAYCOUNT(s_Challenges); i++)
    {
        if (s_Challenges[i].stage == stage && (s32)s_Challenges[i].difficulty > highest)
        {
            highest = s_Challenges[i].difficulty;
        }
    }

    return highest;
}

void challengeStageLoad(LEVELID stage)
{
    g_Challenge = NULL;
    s_Played = NULL;
    s_Passed = FALSE;

    if (!g_ChallengeMenu || gameIsMulti() || g_GameMode != GAMEMODE_SOLO || getPlayerCount() != 1)
    {
        return;
    }

    g_Challenge = challengeFind(stage, lvGetDifficulty());

    if (g_Challenge == NULL)
    {
        return;
    }

    s_Played = g_Challenge;
    s_PassSecs = 0;
    s_State = CHALLENGE_RUNNING;
    s_StartTime = -1;
    s_ResultTicks = RESULT_TICKS;
    s_LevelEnding = FALSE;
    s_Count = 0;
    s_Target = g_Challenge->target;
    s_Chain = 0;
    s_ChainTime = 0;
    s_SpawnTicks = 0;
    s_Spawned = 0;
    s_SpawnGun = ITEM_UNARMED;
    s_ShotActive = FALSE;
    s_ShotHitEnemy = FALSE;
    s_DoorSide = 0.0f;

    // The challenge is the only objective: no "Objective A: Completed" messages
    objectivesDisableStatusDisplay();
}

void challengeStageUnload(void)
{
    g_Challenge = NULL;
}

static s32 challengeTicksLeft(void)
{
    s32 used = (s_StartTime < 0) ? 0 : lvGetMissionTime60() - s_StartTime;

    return g_Challenge->timelimit * TICKS_PER_SEC - used;
}

static void challengeResolve(bool passed)
{
    if (s_State != CHALLENGE_RUNNING)
    {
        return;
    }

    s_State = passed ? CHALLENGE_PASSED : CHALLENGE_FAILED;
    s_Passed = passed;
    s_PassSecs = (g_Challenge->timelimit * TICKS_PER_SEC - challengeTicksLeft() + TICKS_PER_SEC - 1) / TICKS_PER_SEC;
    s_ResultTicks = RESULT_TICKS;

    // Nothing that happens while the result is shown can change it
    g_Mission.playerInvincible = TRUE;
}

static bool challengePlayerInGoalRoom(void)
{
    const PropRecord *prop = currentPlayerGetProp();
    s32 i;

    if (prop == NULL || prop->stan == NULL)
    {
        return FALSE;
    }

    for (i = 0; i < (s32)ARRAYCOUNT(g_Challenge->pads); i++)
    {
        if (g_Challenge->pads[i] >= 0)
        {
            const PadRecord *pad = setupGetPad(g_Challenge->pads[i]);

            if (pad->stan != NULL && pad->stan->room == prop->stan->room)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

// Within this distance of the door (along its face) counts as going through it,
// not round it
#define DOOR_PASS_RADIUS 400.0f
// and this far past its face
#define DOOR_PASS_DEPTH 60.0f

/**
 * Signed distance of the player from the goal door's face. The door's normal is
 * up x look of its pad (its bounding box is thin along that axis); for door 63
 * that is -x, the door's width runs along z.
 */
static f32 challengeDoorDistance(const PropRecord *prop, const PadRecord *pad)
{
    f32 nx = pad->up.y * pad->look.z - pad->up.z * pad->look.y;
    f32 nz = pad->up.x * pad->look.y - pad->up.y * pad->look.x;

    return (prop->pos.x - pad->pos.x) * nx + (prop->pos.z - pad->pos.z) * nz;
}

static bool challengePlayerPastDoor(void)
{
    const PropRecord *prop = currentPlayerGetProp();
    const PadRecord *pad;
    f32 dist;
    f32 dx;
    f32 dz;

    if (g_Challenge->doorpad < 0 || prop == NULL)
    {
        return FALSE;
    }

    pad = setupGetPad(g_Challenge->doorpad);
    dist = challengeDoorDistance(prop, pad);

    if (s_DoorSide == 0.0f)
    {
        s_DoorSide = (dist < 0.0f) ? -1.0f : 1.0f;
        return FALSE;
    }

    if (dist * s_DoorSide > -DOOR_PASS_DEPTH)
    {
        return FALSE;
    }

    dx = prop->pos.x - pad->pos.x;
    dz = prop->pos.z - pad->pos.z;
    return dx * dx + dz * dz < DOOR_PASS_RADIUS * DOOR_PASS_RADIUS;
}

/**
 * Whether CHALLENGE_DESTROY counts the object: one of its targets that can be
 * destroyed (the invincible ones can't be shot, though some can still be blown
 * up, so they are left out either way).
 */
static bool challengeIsTarget(const ObjectRecord *obj)
{
    if ((obj->flags & PROPFLAG_INVINCIBLE) && !(g_Challenge->flags & CF_MAKE_MORTAL))
    {
        return FALSE;
    }

    switch (g_Challenge->targets)
    {
        case CHALLENGE_TARGETS_GLASS:
            return obj->type == PROPDEF_GLASS || obj->type == PROPDEF_TINTED_GLASS;
        case CHALLENGE_TARGETS_AUTOGUNS:
            return obj->type == PROPDEF_AUTOGUN;
        case CHALLENGE_TARGETS_CCTV:
            return obj->type == PROPDEF_CCTV;
        case CHALLENGE_TARGETS_MONITORS:
            return obj->type == PROPDEF_MONITOR || obj->type == PROPDEF_MULTI_MONITOR;
        case CHALLENGE_TARGETS_VEHICLES:
            return obj->type == PROPDEF_VEHICHLE;
        case CHALLENGE_TARGETS_MODELS:
            return obj->obj == g_Challenge->models[0] || (g_Challenge->models[1] != 0 && obj->obj == g_Challenge->models[1]);
    }

    return FALSE;
}

/** How many targets there are to destroy, for a challenge to destroy them all. */
static s32 challengeCountTargets(void)
{
    s32 count = 0;
    PropRecord *prop;

    for (prop = g_ActivePropsHead; prop != NULL; prop = prop->next)
    {
        if (prop->type == PROP_TYPE_OBJ && objIsHealthy(prop->obj) && challengeIsTarget(prop->obj))
        {
            if (g_Challenge->flags & CF_MAKE_MORTAL)
            {
                // Surface 2's full-size Hind is invincible and set into the level
                prop->obj->flags &= ~(PROPFLAG_INVINCIBLE | PROPFLAG_01000000);
            }

            count++;
        }
    }

    return count;
}

static bool chrIsCivilian(const ChrRecord *chr)
{
    return (chr->chrflags & CHRFLAG_COUNT_DEATH_AS_CIVILIAN) != 0;
}

// Survive: at most this many spawned guards alive at once (frame rate)
#define SURVIVE_MAX_ALIVE 8
// a new one every this many ticks while there's room
#define SURVIVE_SPAWN_TICKS (3 * TICKS_PER_SEC)
// at a pad this far from the player (15-45 m)
#define SURVIVE_SPAWN_MIN (15.0f * UNITS_PER_METRE)
#define SURVIVE_SPAWN_MAX (45.0f * UNITS_PER_METRE)
// Spawned guards are numbered from here
#define SURVIVE_CHRNUM 9000

/**
 * Survive: when the clock starts, note where the player is and pick a guard
 * of the level's to copy (body and gun) for the guards that will spawn.
 */
static void challengeSurviveStart(void)
{
    const PropRecord *playerprop = currentPlayerGetProp();
    const PadRecord *pad;
    s32 i;

    s_SurviveCentre = playerprop->pos;

    s_PadCount = 0;

    for (pad = g_CurrentSetup.pads; pad != NULL && pad->plink != NULL; pad++)
    {
        s_PadCount++;
    }

    for (i = 0; i < g_NumChrSlots; i++)
    {
        const ChrRecord *chr = &g_ChrSlots[i];
        const PropRecord *gun;

        if (chr->model == NULL || chr->prop == NULL || chrIsCivilian(chr))
        {
            continue;
        }

        gun = chrGetEquippedWeaponProp(chr, GUNRIGHT);

        if (gun != NULL)
        {
            s_SpawnBody = chr->bodynum;
            s_SpawnGunModel = gun->weapon->obj;
            s_SpawnGun = gun->weapon->weaponnum;
            return;
        }
    }
}

static s32 challengeSurviveAlive(void)
{
    s32 alive = 0;
    s32 i;

    for (i = 0; i < s_Spawned; i++)
    {
        const ChrRecord *chr = chrFindByLiteralId(SURVIVE_CHRNUM + i);

        if (chr != NULL && chr->actiontype != ACT_DIE && chr->actiontype != ACT_DEAD)
        {
            alive++;
        }
    }

    return alive;
}

/**
 * Survive: tops up the guards every few seconds. A new guard appears out of
 * sight at a pad 15 to 45 m away, armed like the level's guards and already
 * after the player.
 */
static void challengeSurviveSpawn(void)
{
    const PropRecord *playerprop = currentPlayerGetProp();
    s32 attempt;

    s_SpawnTicks -= g_ClockTimer;

    if (s_SpawnTicks > 0 || s_SpawnGun == ITEM_UNARMED || s_PadCount == 0)
    {
        return;
    }

    s_SpawnTicks = SURVIVE_SPAWN_TICKS;

    if (challengeSurviveAlive() >= SURVIVE_MAX_ALIVE)
    {
        return;
    }

    for (attempt = 0; attempt < 8; attempt++)
    {
        const PadRecord *pad = &g_CurrentSetup.pads[randomGetNext() % (u32)s_PadCount];
        f32 dx = pad->pos.x - playerprop->pos.x;
        f32 dz = pad->pos.z - playerprop->pos.z;
        f32 distsq = dx * dx + dz * dz;
        const PropRecord *prop;
        ChrRecord *chr;

        if (pad->stan == NULL || distsq < SURVIVE_SPAWN_MIN * SURVIVE_SPAWN_MIN || distsq > SURVIVE_SPAWN_MAX * SURVIVE_SPAWN_MAX)
        {
            continue;
        }

        prop = chrSpawnAtCoord(s_SpawnBody, -1, &pad->pos, pad->stan, mathAtan2(-dx, -dz),
            ailistFindById(GAILIST_STANDARD_GUARD), 0);

        if (prop == NULL)
        {
            continue;
        }

        chr = prop->chr;
        chr->chrnum = SURVIVE_CHRNUM + s_Spawned;
        chrSlotSync(chr);
        s_Spawned++;

        chrGiveWeapon(chr, s_SpawnGunModel, s_SpawnGun, 0);

        // Awake and already after the player
        chrSetTargetToPlayer(chr);
        chrAlertGuardToPlayerPosition(chr);
        return;
    }
}

/** Survive: whether the player has strayed too far from where they started. */
static bool challengeSurviveOutOfArea(void)
{
    const PropRecord *playerprop = currentPlayerGetProp();
    f32 dx = playerprop->pos.x - s_SurviveCentre.x;
    f32 dz = playerprop->pos.z - s_SurviveCentre.z;
    f32 range = g_Challenge->param * UNITS_PER_METRE;

    return g_Challenge->param > 0 && dx * dx + dz * dz > range * range;
}

const PadRecord *challengeGetStartPad(void)
{
    if (g_Challenge == NULL || !(g_Challenge->flags & CF_START_PAD))
    {
        return NULL;
    }

    return &g_CurrentSetup.pads[g_Challenge->pads[0]];
}

/**
 * Once a frame while a challenge runs (from propsTick).
 */
void challengeTick(void)
{
    if (s_State != CHALLENGE_RUNNING)
    {
        // Show the result, then end the level the way a level script does
        if (!s_LevelEnding && g_ClockTimer > 0)
        {
            s_ResultTicks -= g_ClockTimer;

            if (s_ResultTicks <= 0 && !g_CurrentPlayer->bonddead)
            {
                s_LevelEnding = TRUE;
                bossReturnTitleStage();
            }
        }

        return;
    }

    if (g_CurrentPlayer->bonddead)
    {
        challengeResolve(FALSE);
        s_LevelEnding = TRUE; // the death sequence ends the level
        return;
    }

    if (s_StartTime < 0)
    {
        // The clock starts when the intro ends
        if (g_CurrentPlayer->cameramode == CAMERAMODE_INTRO)
        {
            return;
        }

        s_StartTime = lvGetMissionTime60();

        if (g_Challenge->count == CHALLENGE_DESTROY && s_Target == 0)
        {
            s_Target = challengeCountTargets();
        }

        if (g_Challenge->count == CHALLENGE_SURVIVE)
        {
            challengeSurviveStart();
        }
    }

    if (challengeTicksLeft() <= 0)
    {
        // Survive passes by lasting until the end
        challengeResolve(g_Challenge->count == CHALLENGE_SURVIVE);
        return;
    }

    if (g_Challenge->count == CHALLENGE_SURVIVE)
    {
        if (challengeSurviveOutOfArea())
        {
            challengeResolve(FALSE);
        }
        else
        {
            challengeSurviveSpawn();
        }
    }
    else if (g_Challenge->count == CHALLENGE_REACH)
    {
        if (challengePlayerInGoalRoom() || challengePlayerPastDoor())
        {
            challengeResolve(TRUE);
        }
    }
    else if (s_Count >= s_Target)
    {
        challengeResolve(TRUE);
    }
}

/**
 * The challenge line at the top of the screen: its name, progress and time left,
 * or the result once decided.
 */
Gfx *challengeRender(Gfx *gdl)
{
    char text[64];
    s32 ticks;
    s32 secs;
    s32 y = viGetViewTop() + 14;

    if (g_CurrentPlayer->cameramode == CAMERAMODE_INTRO)
    {
        return gdl;
    }

    if (s_State == CHALLENGE_PASSED)
    {
        sprintf(text, "CHALLENGE COMPLETE\n");
    }
    else if (s_State == CHALLENGE_FAILED)
    {
        sprintf(text, "CHALLENGE FAILED\n");
    }
    else
    {
        ticks = challengeTicksLeft();
        secs = (ticks + TICKS_PER_SEC - 1) / TICKS_PER_SEC;

        if (g_Challenge->count != CHALLENGE_REACH && g_Challenge->count != CHALLENGE_KILL_CHR && g_Challenge->count != CHALLENGE_PHOTO
            && g_Challenge->count != CHALLENGE_SURVIVE)
        {
            sprintf(text, "%s  %d OF %d  %d:%02d\n", g_Challenge->name, s_Count, s_Target, secs / 60, secs % 60);
        }
        else
        {
            sprintf(text, "%s  %d:%02d\n", g_Challenge->name, secs / 60, secs % 60);
        }
    }

    gdl = textSetupDraw(gdl);
    gdl = gunDrawHudString(gdl, text, 160, HUDHALIGN_MIDDLE, y, HUDVALIGN_TOP, TRUE);
    return textEndDraw(gdl);
}

// Events count once the clock has started, until the result is decided
static bool challengeIsLive(void)
{
    return s_State == CHALLENGE_RUNNING && s_StartTime >= 0;
}

NOINLINE void challengeOnHatKnockedOff(void)
{
    if (challengeIsLive() && g_Challenge->count == CHALLENGE_HATS)
    {
        s_Count++;
    }
}

/**
 * A player's bullet (or shotgun blast) is about to be resolved. Its hits are
 * applied before challengeOnShotEnd.
 */
NOINLINE void challengeOnShotBegin(void)
{
    s_ShotActive = TRUE;
    s_ShotHitEnemy = FALSE;
}

NOINLINE void challengeOnShotEnd(void)
{
    s_ShotActive = FALSE;

    // Marksman: anything but an enemy (a wall, glass, a scientist) is a miss
    if ((g_Challenge->flags & CF_FAIL_MISS) && !s_ShotHitEnemy && challengeIsLive())
    {
        challengeResolve(FALSE);
    }
}

NOINLINE void challengeOnPlayerHitChr(const ChrRecord *chr)
{
    if (s_ShotActive && !chrIsCivilian(chr))
    {
        s_ShotHitEnemy = TRUE;
    }
}

NOINLINE void challengeOnPlayerDamaged(void)
{
    if ((g_Challenge->flags & CF_FAIL_DAMAGE) && challengeIsLive())
    {
        challengeResolve(FALSE);
    }
}

/**
 * The player hurt a chr: a shot that did damage, or any explosion (in solo the
 * explosions that matter are the player's).
 */
NOINLINE void challengeOnChrHarmed(const ChrRecord *chr)
{
    (void)chr;

    if ((g_Challenge->flags & CF_FAIL_HARM) && challengeIsLive())
    {
        challengeResolve(FALSE);
    }
}

/**
 * A chr died from the player's shot (hitpart is where it hit) or from an
 * explosion (explosion is TRUE, hitpart -1). Explosions include the tank
 * running someone over.
 */
NOINLINE void challengeOnChrKilled(const ChrRecord *chr, s32 hitpart, bool explosion)
{
    const PropRecord *playerprop;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 range;

    if (!challengeIsLive())
    {
        return;
    }

    if (g_Challenge->flags & CF_FAIL_HARM)
    {
        challengeResolve(FALSE);
        return;
    }

    if (g_Challenge->count == CHALLENGE_KILL_CHR)
    {
        if (chr->chrnum == g_Challenge->param)
        {
            s_Count++;
        }

        return;
    }

    if (chrIsCivilian(chr))
    {
        return;
    }

    if ((g_Challenge->flags & CF_FAIL_BODYKILL) && (explosion || hitpart != HIT_HEAD))
    {
        challengeResolve(FALSE);
        return;
    }

    switch (g_Challenge->count)
    {
        case CHALLENGE_KILLS:
            s_Count++;
            break;
        case CHALLENGE_HEAD_KILLS:
            if (!explosion && hitpart == HIT_HEAD)
            {
                s_Count++;
            }
            break;
        case CHALLENGE_FAR_KILLS:
            playerprop = currentPlayerGetProp();
            dx = chr->prop->pos.x - playerprop->pos.x;
            dy = chr->prop->pos.y - playerprop->pos.y;
            dz = chr->prop->pos.z - playerprop->pos.z;
            range = g_Challenge->param * UNITS_PER_METRE;

            if (dx * dx + dy * dy + dz * dz >= range * range)
            {
                s_Count++;
            }
            break;
        case CHALLENGE_TANK_KILLS:
            if (playerIsInTank())
            {
                s_Count++;
            }
            break;
        case CHALLENGE_CHAIN:
            if (explosion)
            {
                if (s_Chain > 0 && g_GlobalTimer - s_ChainTime <= CHAIN_TICKS)
                {
                    s_Chain++;
                }
                else
                {
                    s_Chain = 1;
                }

                s_ChainTime = g_GlobalTimer;

                // The best chain so far
                if (s_Chain > s_Count)
                {
                    s_Count = s_Chain;
                }
            }
            break;
        default:
            break;
    }
}

/** An enemy noticed the player (chrCheckTargetInSight). */
NOINLINE void challengeOnChrSawPlayer(const ChrRecord *chr)
{
    if ((g_Challenge->flags & CF_FAIL_SEEN) && challengeIsLive() && !chrIsCivilian(chr))
    {
        challengeResolve(FALSE);
    }
}

NOINLINE void challengeOnAlarm(void)
{
    if ((g_Challenge->flags & CF_FAIL_ALARM) && challengeIsLive())
    {
        challengeResolve(FALSE);
    }
}

/** The object was just destroyed (by anyone). */
NOINLINE void challengeOnObjDestroyed(const ObjectRecord *obj)
{
    if (g_Challenge->count == CHALLENGE_DESTROY && challengeIsLive() && challengeIsTarget(obj))
    {
        s_Count++;
    }
}

// A photo only counts from this close (20 m), so the target isn't a speck
#define PHOTO_RANGE (20.0f * UNITS_PER_METRE)

/**
 * The player took a picture with the camera: it counts if the target chr is
 * alive, in view and close enough.
 */
NOINLINE void challengeOnPhoto(void)
{
    const ChrRecord *chr;
    const PropRecord *playerprop = currentPlayerGetProp();
    f32 dx;
    f32 dy;
    f32 dz;

    if (g_Challenge->count != CHALLENGE_PHOTO || !challengeIsLive())
    {
        return;
    }

    chr = chrFindByLiteralId(g_Challenge->param);

    if (chr == NULL || chr->prop == NULL || chr->actiontype == ACT_DIE || chr->actiontype == ACT_DEAD
        || !(chr->prop->flags & PROPFLAG_ONSCREEN) || chr->prop->zDepth < 0.0f)
    {
        return;
    }

    dx = chr->prop->pos.x - playerprop->pos.x;
    dy = chr->prop->pos.y - playerprop->pos.y;
    dz = chr->prop->pos.z - playerprop->pos.z;

    if (dx * dx + dy * dy + dz * dz < PHOTO_RANGE * PHOTO_RANGE)
    {
        s_Count++;
    }
}

/** With CF_NO_PICKUPS, only the loadout's weapons can be picked up. */
NOINLINE bool challengeBlocksPickup(ITEM_IDS weapon)
{
    if (!(g_Challenge->flags & CF_NO_PICKUPS))
    {
        return FALSE;
    }

    if (weapon < ITEM_KNIFE || weapon > ITEM_REMOTEMINE || weapon == ITEM_WATCHLASER)
    {
        return FALSE; // not a weapon: keys, mission items
    }

    return weapon != g_Challenge->items[0] && weapon != g_Challenge->items[1] && weapon != g_Challenge->items[2];
}

static void challengeGiveItem(ITEM_IDS item)
{
    s32 ammotype = weaponGetAmmoType(item);

    weaponLoadProjectileModels(item);
    invAddItem(item);

    if (item == ITEM_REMOTEMINE)
    {
        invAddItem(ITEM_TRIGGER);
    }

    if (ammotype != AMMO_NONE)
    {
        ammoGive(ammotype, ammoGetMax(ammotype));
    }
}

/**
 * The challenge's weapons, given after the level's own start weapons. They
 * replace them (CF_REPLACE) or are added; the player starts holding them
 * unless CF_KEEP_GUN.
 */
void challengeApplyLoadout(void)
{
    ITEM_IDS right = g_Challenge->items[0];
    ITEM_IDS left = g_Challenge->items[1];
    s32 i;

    if (g_Challenge->flags & CF_REPLACE)
    {
        invResetKeepProps();

        for (i = 0; i < (s32)ARRAYCOUNT(g_CurrentPlayer->ammoheldarr); i++)
        {
            g_CurrentPlayer->ammoheldarr[i] = 0;
        }

        starting_weapon[GUNRIGHT] = ITEM_UNARMED;
        starting_weapon[GUNLEFT] = ITEM_UNARMED;
    }

    if (right == ITEM_UNARMED)
    {
        return;
    }

    challengeGiveItem(right);

    if (left != ITEM_UNARMED)
    {
        if (left != right)
        {
            challengeGiveItem(left);
        }

        invAddDualItem(right, left);
    }

    if (g_Challenge->items[2] != ITEM_UNARMED)
    {
        challengeGiveItem(g_Challenge->items[2]);
    }

    if (!(g_Challenge->flags & CF_KEEP_GUN))
    {
        starting_weapon[GUNRIGHT] = right;
        starting_weapon[GUNLEFT] = left;
    }
}

/**
 * The level is ending (bossReturnTitleStage). If the challenge hasn't been
 * decided yet, the level's own exit passes it with CF_EXIT, and anything else
 * fails it.
 */
void challengeOnLevelEnd(void)
{
    s_LevelEnding = TRUE;
    challengeResolve((g_Challenge->flags & CF_EXIT) && s_StartTime >= 0 && !g_CurrentPlayer->bonddead);
}

// What the objective line calls each kind of destroy target
static const char *const s_TargetNouns[] = {
    [CHALLENGE_TARGETS_GLASS] = "panes of glass",
    [CHALLENGE_TARGETS_AUTOGUNS] = "drone guns",
    [CHALLENGE_TARGETS_CCTV] = "security cameras",
    [CHALLENGE_TARGETS_MONITORS] = "monitors",
    [CHALLENGE_TARGETS_VEHICLES] = "vehicles",
    [CHALLENGE_TARGETS_MODELS] = "targets",
};

/**
 * The challenge as a mission objective, for the briefing, the watch and the
 * debrief: the goal (the table's own, or one made from the count), how it
 * must be done, and the time limit. Writes it to buffer, ending in a newline.
 */
char *challengeGetObjectiveText(const ChallengeDef *def, char *buffer)
{
    char *p = buffer;
    u16 flags = def->flags;

    if (def->goal != NULL)
    {
        p += sprintf(p, def->goal, def->target);
    }
    else
    {
        switch (def->count)
        {
            case CHALLENGE_REACH:
                p += sprintf(p, "Reach the exit");
                break;
            case CHALLENGE_HATS:
                p += sprintf(p, "Knock off %d hats", def->target);
                break;
            case CHALLENGE_KILLS:
                p += sprintf(p, def->target == 1 ? "Kill %d enemy" : "Kill %d enemies", def->target);
                break;
            case CHALLENGE_FAR_KILLS:
                p += sprintf(p, "Kill %d enemies from %d m or more away", def->target, def->param);
                break;
            case CHALLENGE_HEAD_KILLS:
                p += sprintf(p, "Kill %d enemies with head shots", def->target);
                flags &= ~CF_FAIL_BODYKILL; // already said
                break;
            case CHALLENGE_CHAIN:
                p += sprintf(p, "Kill %d enemies in one chain of explosions", def->target);
                break;
            case CHALLENGE_TANK_KILLS:
                p += sprintf(p, "Kill %d enemies with the tank", def->target);
                break;
            case CHALLENGE_DESTROY:
                if (def->target == 0)
                {
                    p += sprintf(p, "Destroy all %s", s_TargetNouns[def->targets]);
                }
                else
                {
                    p += sprintf(p, "Destroy %d %s", def->target, s_TargetNouns[def->targets]);
                }
                break;
            case CHALLENGE_KILL_CHR:
                p += sprintf(p, "Kill the target");
                break;
            case CHALLENGE_PHOTO:
                p += sprintf(p, "Photograph the target");
                break;
            case CHALLENGE_SURVIVE:
                p += sprintf(p, "Hold your ground");
                break;
        }
    }

    if (flags & CF_FAIL_SEEN)
    {
        p += sprintf(p, " without being seen");
    }
    else if (flags & CF_FAIL_ALARM)
    {
        p += sprintf(p, " without setting off an alarm");
    }

    if (flags & CF_FAIL_DAMAGE)
    {
        p += sprintf(p, " without taking damage");
    }

    if (flags & CF_FAIL_HARM)
    {
        p += sprintf(p, " without hurting anyone");
    }

    if (flags & CF_FAIL_MISS)
    {
        p += sprintf(p, " without missing a shot");
    }

    if (flags & CF_FAIL_BODYKILL)
    {
        p += sprintf(p, " with head shots only");
    }

    if (flags & CF_NO_PICKUPS)
    {
        p += sprintf(p, " with only the weapons given");
    }

    if (def->count == CHALLENGE_SURVIVE)
    {
        p += sprintf(p, " for %d:%02d\n", def->timelimit / 60, def->timelimit % 60);
    }
    else
    {
        p += sprintf(p, " within %d:%02d\n", def->timelimit / 60, def->timelimit % 60);
    }

    return buffer;
}

/** The objective text of the challenge played last (also after the stage unloads). */
char *challengeGetPlayedObjectiveText(char *buffer)
{
    buffer[0] = '\0';
    return s_Played != NULL ? challengeGetObjectiveText(s_Played, buffer) : buffer;
}

/** How the challenge is going, as an objective status (OBJECTIVESTATUS_...). */
s32 challengeGetObjectiveStatus(void)
{
    if (s_Played == NULL || (g_Challenge != NULL && s_State == CHALLENGE_RUNNING))
    {
        return OBJECTIVESTATUS_INCOMPLETE;
    }

    return s_Passed ? OBJECTIVESTATUS_COMPLETE : OBJECTIVESTATUS_FAILED;
}

bool challengeWasPlayed(void)
{
    return s_Played != NULL;
}

bool challengeWasPassed(void)
{
    return s_Played != NULL && s_Passed;
}

s32 challengeGetPassSeconds(void)
{
    return s_PassSecs;
}

const char *challengeGetResultName(void)
{
    return s_Played != NULL ? s_Played->name : "";
}
