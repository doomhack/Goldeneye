/* The game's limits.h also has FLT_MAX, for the host tests. */
#include_next <limits.h>
#ifndef FLT_MAX
#define FLT_MAX __FLT_MAX__
#endif
