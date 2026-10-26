#include <ultra64.h>
#include "game/level/compiletime.h"

char *get_compile_time(void) {
    return __DATE__" " __TIME__;
}

