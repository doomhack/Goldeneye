#ifndef _CHALLENGE_H_
#define _CHALLENGE_H_
#include <ultra64.h>
#include <bondtypes.h>

/**
 * Challenge mode: a solo mission with one objective in place of the normal
 * ones. A challenge is picked by stage and difficulty from a table in
 * challenge.c. The level loads and plays as normal; the challenge watches a
 * few game events, shows its progress and time left at the top of the screen,
 * and ends the level when it is passed or failed.
 *
 * A challenge runs when the player picks Challenges on the mode select page
 * (g_ChallengeMenu) and then a level and difficulty that have one.
 *
 * Prototype: Facility only, no saving.
 *
 * Every hook outside this file is guarded by g_Challenge, which is NULL unless
 * a challenge is running, so other modes pay one load and branch per event.
 */

typedef enum ChallengeType
{
    CHALLENGE_HAT_TRICK,    // knock off target hats
    CHALLENGE_MARKSMAN,     // reach the exit; every shot must hit an enemy
    CHALLENGE_UNTOUCHABLE   // get past a door without taking damage
} ChallengeType;

typedef struct ChallengeDef
{
    LEVELID stage;
    DIFFICULTY difficulty;
    ChallengeType type;
    s16 timelimit;          // seconds
    s16 target;             // hats to knock off
    s16 pads[2];            // the goal: reaching the room of either pad (-1: none)
    s16 doorpad;            // or: getting past the door on this (bound) pad (-1: none)
    const char *name;       // shown at the top of the screen and on the debrief
} ChallengeDef;

typedef enum ChallengeState
{
    CHALLENGE_RUNNING,
    CHALLENGE_PASSED,
    CHALLENGE_FAILED
} ChallengeState;

extern const ChallengeDef *g_Challenge;

// Challenges was picked on the mode select page (until the player goes back there)
extern bool g_ChallengeMenu;

// The challenge for a stage and difficulty, or NULL
const ChallengeDef *challengeFind(LEVELID stage, DIFFICULTY difficulty);
// The hardest difficulty with a challenge on the stage, or -1
s32 challengeGetHighestDifficulty(LEVELID stage);

void challengeStageLoad(LEVELID stage);
void challengeStageUnload(void);
void challengeTick(void);
Gfx *challengeRender(Gfx *gdl);

// Event hooks, only called while g_Challenge is set
void challengeOnHatKnockedOff(void);
void challengeOnShotBegin(void);
void challengeOnShotEnd(void);
void challengeOnPlayerHitChr(const ChrRecord *chr);
void challengeOnPlayerDamaged(void);
void challengeOnLevelEnd(void);

// For the debrief, which runs after the stage has unloaded
bool challengeWasPlayed(void);
bool challengeWasPassed(void);
const char *challengeGetResultName(void);

#endif
