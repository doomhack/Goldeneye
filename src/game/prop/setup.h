#ifndef _SETUP_H_
#define _SETUP_H_
#include <ultra64.h>
#include <bondtypes.h>

/* The level's setup: the working copy (pointers into the loaded file), and the file */
extern stagesetup g_CurrentSetup;
extern stagesetup *g_ptrStageSetupFile;

/**
 * A pad by number: plain pads and bound pads start the same way (position, up, look),
 * so a bound pad can be used as a plain one.
 */
static inline PadRecord *setupGetPad(s32 padnum)
{
    if (isNotBoundPad(padnum))
    {
        return &g_CurrentSetup.pads[padnum];
    }

    return (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
}

void setupCreateProps(enum LEVELID stageId);
void setupFreeProps(void);
void setupGuard(GuardRecord *guard);

s32                  sizepropdef(PropDefHeaderRecord *pdef);
PropDefHeaderRecord *setupGetPtrToCommandByIndex(s32 index);
ObjectRecord        *setupCommandGetObject(s32 index);
s32                  setupGetCommandIndexByProp(PropRecord *prop);
s32                  tagGetCommandIndex(ObjectRecord *tag);
s32                  modelLoad(s32 modelid);
ObjectRecord        *setupFindObjForReuse(s32 wanttype, ObjectRecord **offscreenobjptr, ObjectRecord **anyobjptr, bool musthaveprop, bool musthavemodel, ModelFileHeader *modeldef);

void propsResetLevelState(void);
void objSoundsInit(void);
void objSoundsStopAll(void);

/* objsetup.c: creating each kind of object */
#include "game/prop/objsetup.h"

#endif
