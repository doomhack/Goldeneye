/**
 * Challenge mode (prototype): see challenge.h.
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
#include "fr.h"
#include "str.h"

// The game clock runs at 60 ticks a second in every region
#define TICKS_PER_SEC 60

// How long the result stays on screen before the level ends
#define RESULT_TICKS (2 * TICKS_PER_SEC)

[[maybe_unused]] static const ChallengeDef s_Challenges[] = {
    // Facility (Zak's list, 2026-11-14). Pads 0x87 and 0x135 are the rooms the
    // level's own script treats as the exit (Facility ai_47). Bound pad 93 is the
    // remotely locked security door (door 63) on the lower floor.
    { LEVELID_FACILITY, DIFFICULTY_AGENT,  CHALLENGE_HAT_TRICK,   90,  3, { -1, -1 },      -1,    "HAT TRICK" },
    { LEVELID_FACILITY, DIFFICULTY_SECRET, CHALLENGE_MARKSMAN,    120, 0, { 0x87, 0x135 }, -1,    "MARKSMAN" },
    { LEVELID_FACILITY, DIFFICULTY_00,     CHALLENGE_UNTOUCHABLE, 60,  0, { -1, -1 },      10093, "UNTOUCHABLE" },
};

const ChallengeDef *g_Challenge = NULL;

static ChallengeState s_State;
static s32 s_StartTime;         // mission time (ticks) when the player got control, -1 before
static s32 s_ResultTicks;       // counts down the result message once decided
static bool s_LevelEnding;      // the level end has been requested
static s32 s_Hats;
static bool s_ShotActive;       // a player shot is being resolved
static bool s_ShotHitEnemy;     // and it hit an enemy
static f32 s_DoorSide;          // which side of the goal door the player started on (+1/-1)

// What the debrief shows, kept after the stage unloads
static const ChallengeDef *s_Played;
static bool s_Passed;

void challengeStageLoad(LEVELID stage)
{
    s32 i;

    g_Challenge = NULL;
    s_Played = NULL;
    s_Passed = FALSE;

#ifdef CHALLENGE_MODE
    if (gameIsMulti() || g_GameMode != GAMEMODE_SOLO || getPlayerCount() != 1)
    {
        return;
    }

    for (i = 0; i < (s32)ARRAYCOUNT(s_Challenges); i++)
    {
        if (s_Challenges[i].stage == stage && s_Challenges[i].difficulty == lvGetDifficulty())
        {
            g_Challenge = &s_Challenges[i];
            break;
        }
    }

    if (g_Challenge == NULL)
    {
        return;
    }

    s_Played = g_Challenge;
    s_State = CHALLENGE_RUNNING;
    s_StartTime = -1;
    s_ResultTicks = RESULT_TICKS;
    s_LevelEnding = FALSE;
    s_Hats = 0;
    s_ShotActive = FALSE;
    s_ShotHitEnemy = FALSE;
    s_DoorSide = 0.0f;

    // The challenge is the only objective: no "Objective A: Completed" messages
    objectivesDisableStatusDisplay();
#else
    (void)stage;
    (void)i;
#endif
}

void challengeStageUnload(void)
{
    g_Challenge = NULL;
}

static void challengeResolve(bool passed)
{
    if (s_State != CHALLENGE_RUNNING)
    {
        return;
    }

    s_State = passed ? CHALLENGE_PASSED : CHALLENGE_FAILED;
    s_Passed = passed;
    s_ResultTicks = RESULT_TICKS;

    // Nothing that happens while the result is shown can change it
    g_Mission.playerInvincible = TRUE;
}

static s32 challengeTicksLeft(void)
{
    s32 used = (s_StartTime < 0) ? 0 : lvGetMissionTime60() - s_StartTime;

    return g_Challenge->timelimit * TICKS_PER_SEC - used;
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
    }

    if (challengeTicksLeft() <= 0)
    {
        challengeResolve(FALSE);
        return;
    }

    switch (g_Challenge->type)
    {
        case CHALLENGE_HAT_TRICK:
            if (s_Hats >= g_Challenge->target)
            {
                challengeResolve(TRUE);
            }
            break;
        case CHALLENGE_MARKSMAN:
        case CHALLENGE_UNTOUCHABLE:
            if (challengePlayerInGoalRoom() || challengePlayerPastDoor())
            {
                challengeResolve(TRUE);
            }
            break;
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

        if (g_Challenge->type == CHALLENGE_HAT_TRICK)
        {
            sprintf(text, "%s  %d OF %d  %d:%02d\n", g_Challenge->name, s_Hats, g_Challenge->target, secs / 60, secs % 60);
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

void challengeOnHatKnockedOff(void)
{
    if (s_State == CHALLENGE_RUNNING)
    {
        s_Hats++;
    }
}

/**
 * A player's bullet (or shotgun blast) is about to be resolved. Its hits are
 * applied before challengeOnShotEnd.
 */
void challengeOnShotBegin(void)
{
    s_ShotActive = TRUE;
    s_ShotHitEnemy = FALSE;
}

void challengeOnShotEnd(void)
{
    s_ShotActive = FALSE;

    // Marksman: anything but an enemy (a wall, glass, a scientist) is a miss
    if (g_Challenge->type == CHALLENGE_MARKSMAN && !s_ShotHitEnemy && s_StartTime >= 0)
    {
        challengeResolve(FALSE);
    }
}

void challengeOnPlayerHitChr(const ChrRecord *chr)
{
    if (s_ShotActive && !(chr->chrflags & CHRFLAG_COUNT_DEATH_AS_CIVILIAN))
    {
        s_ShotHitEnemy = TRUE;
    }
}

void challengeOnPlayerDamaged(void)
{
    if (g_Challenge->type == CHALLENGE_UNTOUCHABLE)
    {
        challengeResolve(FALSE);
    }
}

/**
 * The level is ending (bossReturnTitleStage). If the challenge hasn't been
 * decided yet, ending the level some other way fails it.
 */
void challengeOnLevelEnd(void)
{
    s_LevelEnding = TRUE;
    challengeResolve(FALSE);
}

bool challengeWasPlayed(void)
{
    return s_Played != NULL;
}

bool challengeWasPassed(void)
{
    return s_Played != NULL && s_Passed;
}

const char *challengeGetResultName(void)
{
    return s_Played != NULL ? s_Played->name : "";
}
