#ifndef _TESTHOOKS_H_
#define _TESTHOOKS_H_
#include <ultra64.h>

/*
 * Hooks for the headless test runner (tools/testrun). They do nothing unless the
 * runner writes to these variables in RAM, so test runs use the same ROM that ships.
 * TEST_HOOK_VAR keeps them as they are with LTO: the runner reads and writes them
 * from outside, which GCC can't see when it optimises the whole game at once.
 */
#define TEST_HOOK_VAR __attribute__((used, externally_visible))

/* Set to 1 by the runner to log a state checksum every game frame. */
extern s32 g_TestHooksEnabled;

/* Number of entries written to g_TestStateLog so far (it wraps every TEST_STATE_LOG_LEN). */
extern u32 g_TestStateLogCount;

#define TEST_STATE_LOG_LEN 64

/* Per frame: the game timer, an exact checksum and a coarse checksum.
 * The exact one covers float bit patterns; the coarse one rounds positions to
 * whole units and health to 1%, so tiny float differences don't show up. */
extern u32 g_TestStateLog[TEST_STATE_LOG_LEN][3];

/* Attract-mode demo to play instead of a random one (index into ramrom_table), or -1. */
extern s32 g_TestForceDemo;

/* Set when a demo stops because the game no longer matches the recording
 * (its random seed or input checksum differs). */
extern s32 g_TestDemoDesynced;

void testhooksEndFrame(void);

#endif
