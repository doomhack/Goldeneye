#include <ultra64.h>
#include <bondgame.h>
#include "game/model/chrobjdata.h"

#include <assets/obseg/prop/propModelFileHeaders.inc.c>

#include <assets/embedded/skeletons/props.inc.c>



//D:8003A228                     .globl PitemZ_entries
#include <assets/obseg/prop/propItemModelFileRecord.inc.c>

#if defined(VERSION_EU)
#include <assets/obseg/prop/propExplosionDetailsRecords-eu.inc.c>
#else
#include <assets/obseg/prop/propExplosionDetailsRecords.inc.c>
#endif
