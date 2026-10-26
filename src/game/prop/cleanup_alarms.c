#include <ultra64.h>
#include "game/chr/chrai.h"
#include "game/prop/cleanup_alarms.h"


void cleanupAlarms(void) {
    alarmDeactivate();
    check_deactivate_gas_sound();
}


