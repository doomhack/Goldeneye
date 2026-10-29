#ifndef _MINE_H_
#define _MINE_H_

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

/* A primed proximity mine goes off when the player or a guard comes within 2.5 m */
#define PROXMINE_TRIGGER_DIST_SQ (250.0f * 250.0f)
#define MAX_PROXMINES            30

extern s32 g_RemoteMineOwnerTriggerFlag;
extern WeaponObjRecord *g_ProxMines[MAX_PROXMINES];

void remoteMinesDetonate(void);

void proxminesReset(void);
void proxmineAdd(WeaponObjRecord *mine);
void proxmineRemove(WeaponObjRecord *mine);
void proxminesDetonateNear(coord3d *pos);
void proxminesTickGuards(void);
void objTickExplosive(PropRecord *prop);

#endif
