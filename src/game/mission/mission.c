/**
 * Mission-wide state (struct Mission). See docs/subsystems/mission.md.
 */
#include <ultra64.h>
#include "game/mission/mission.h"

struct Mission g_Mission = {
    .playerHasControl = TRUE,
};
