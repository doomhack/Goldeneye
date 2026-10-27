#include <ultra64.h>
#include "game/level/compiletime.h"

char *gameGetCompileTime(void) {
    return __DATE__" " __TIME__;
}

