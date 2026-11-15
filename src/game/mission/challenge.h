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
 * Every hook outside this file is guarded by g_Challenge, which is NULL unless
 * a challenge is running, so other modes pay one load and branch per event.
 */

/**
 * What a challenge counts. CHALLENGE_REACH has no count: it passes when the
 * player reaches the room of one of its pads, gets past its door, or (with
 * CF_EXIT) when the level's own script ends the level.
 */
typedef enum ChallengeCount
{
    CHALLENGE_REACH,
    CHALLENGE_HATS,         // knock off hats
    CHALLENGE_KILLS,        // kill enemies (not civilians)
    CHALLENGE_FAR_KILLS,    // kill enemies at least param metres away
    CHALLENGE_HEAD_KILLS,   // kill enemies with a head shot
    CHALLENGE_CHAIN,        // kill enemies with explosions, each within a second of the last
    CHALLENGE_TANK_KILLS,   // kill enemies while driving the tank
    CHALLENGE_DESTROY,      // destroy objects (see ChallengeTargets)
    CHALLENGE_KILL_CHR,     // kill the chr numbered param
    CHALLENGE_PHOTO,        // photograph the chr numbered param with the camera
    CHALLENGE_SURVIVE       // stay alive (within param metres of the start) while guards keep coming
} ChallengeCount;

// Which objects CHALLENGE_DESTROY counts
typedef enum ChallengeTargets
{
    CHALLENGE_TARGETS_GLASS,    // glass and tinted glass
    CHALLENGE_TARGETS_AUTOGUNS, // drone guns
    CHALLENGE_TARGETS_CCTV,     // security cameras
    CHALLENGE_TARGETS_MONITORS, // single and multi monitors
    CHALLENGE_TARGETS_VEHICLES, // cars and trucks
    CHALLENGE_TARGETS_MODELS    // any object whose model is in models[]
} ChallengeTargets;

// ChallengeDef flags
enum
{
    CF_EXIT          = 0x0001, // the level's own end (an exit script) passes
    CF_FAIL_SEEN     = 0x0002, // fails when an enemy notices the player
    CF_FAIL_ALARM    = 0x0004, // fails when an alarm goes off
    CF_FAIL_DAMAGE   = 0x0008, // fails when the player takes damage
    CF_FAIL_HARM     = 0x0010, // fails when the player hurts anyone
    CF_FAIL_MISS     = 0x0020, // fails when a shot hits anything but an enemy
    CF_FAIL_BODYKILL = 0x0040, // fails on a kill that isn't a head shot
    CF_REPLACE       = 0x0080, // the loadout replaces the level's start weapons
    CF_KEEP_GUN      = 0x0100, // the loadout is added, but the player starts with the level's gun
    CF_NO_PICKUPS    = 0x0200, // weapons other than the loadout can't be picked up
    CF_MAKE_MORTAL   = 0x0400, // DESTROY: invincible targets can be destroyed too
    CF_START_PAD     = 0x0800  // the player starts at pads[0] instead of the level's start
};

typedef struct ChallengeDef
{
    LEVELID stage;
    DIFFICULTY difficulty;
    ChallengeCount count;
    ChallengeTargets targets;   // CHALLENGE_DESTROY
    u16 flags;
    s16 timelimit;              // seconds
    s16 target;                 // how many to count (CHALLENGE_DESTROY: 0 for all there are)
    s16 param;                  // FAR_KILLS: metres; KILL_CHR: chr number
    s16 pads[2];                // REACH: the room of either pad (-1: none); CF_START_PAD: pads[0] is the start
    s16 doorpad;                // REACH: or getting past the door on this (bound) pad (-1: none)
    s16 models[2];              // TARGETS_MODELS: object models (0: none)
    s8 items[3];                // loadout: right and left hand, and one more item (ITEM_UNARMED: none)
    const char *name;           // shown at the top of the screen and on the debrief
    const char *goal;           // the objective's goal (may use %d for target); NULL to make one
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
void challengeOnChrKilled(const ChrRecord *chr, s32 hitpart, bool explosion);
void challengeOnChrHarmed(const ChrRecord *chr);
void challengeOnChrSawPlayer(const ChrRecord *chr);
void challengeOnAlarm(void);
void challengeOnObjDestroyed(const ObjectRecord *obj);
bool challengeBlocksPickup(ITEM_IDS weapon);
void challengeOnPhoto(void);
// After the level's start weapons are given (once per stage load)
void challengeApplyLoadout(void);
// The pad the player starts at instead of the level's own start, or NULL
const PadRecord *challengeGetStartPad(void);

// For the debrief, which runs after the stage has unloaded
bool challengeWasPlayed(void);
bool challengeWasPassed(void);
const char *challengeGetResultName(void);
s32 challengeGetPassSeconds(void);

// The challenge as a mission objective (briefing, watch, debrief)
char *challengeGetObjectiveText(const ChallengeDef *def, char *buffer);
char *challengeGetPlayedObjectiveText(char *buffer);
s32 challengeGetObjectiveStatus(void);

#endif
