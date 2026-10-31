#ifndef _MISSION_H_
#define _MISSION_H_
#include <ultra64.h>

/**
 * Mission-wide state, mostly set by AI scripts. Reset at level start
 * (playerApplyIntro).
 */
struct Mission
{
    s32 exitOnPress;           // end the level on the next button press: 1 waiting, 2 pressed (fading out)
    s32 playerHasControl;      // FALSE while AI scripts take the player's controls away
    bool playerInvincible;     // no damage and no pickups (cutscenes, the end of a level)
    s32 surroundWithExplosions; // explosions round the player (playerSurroundWithExplosions)
    s32 explodeTankOnDeath;    // the player died in the tank: blow it up with the death camera
};

extern struct Mission g_Mission;

#endif
