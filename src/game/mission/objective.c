/**
 * Mission objectives and briefings. While the setup file is read, its tags, briefing
 * texts, objectives and room/deposit/photo criteria are linked into lists here
 * (tagAdd, objectiveAdd...). objectiveCheckStatus works out an objective's status from its
 * criteria each time it is asked, and objectivesTickStatusMessages shows "Objective A:
 * completed" when a status changes. See docs/subsystems/mission.md.
 */

#include <ultra64.h>
#include <boss.h>
#include "game/mission/objective.h"
#include <bondconstants.h>
#include <bondtypes.h>
#include "assets/obseg/text/LmiscE.h"
#include "game/text/language.h"
#include "PR/os.h"
#include "str.h"
#include "game/player/bondview.h"
#include "game/player/player.h"
#include "game/player/bondinv.h"
#include "bondtypes.h"



/*
 * Clears Objectives and states.
 *
 * NTSC address 0x7F004EC0.
 */
void objectivesReset(void)
{
    s32 i;


    if (bossGetStageNum() != LEVELID_TITLE)
    {
        g_ObjectiveLastIndex = -1;

        for (i = 0; i < OBJECTIVES_MAX; i++)
        {
            g_ObjectiveStatuses[i] = OBJECTIVESTATUS_INCOMPLETE;
        }
    }

    g_ObjectiveStatusDisplayDisabled = FALSE;

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        g_Objectives[i] = NULL;
    }
    
    g_Tags = NULL;
    g_BriefingTexts = NULL;
    g_RoomEnteredCriteria = NULL;
    g_DepositCriteria = NULL;
    g_PictureCriteria = NULL;
}

void tagAdd(struct TagObjectRecord *arg0)
{
    arg0->NextTag = g_Tags;
    g_Tags = arg0;
}

void briefingTextAdd(struct setup_objective_text *arg0)
{
    arg0->next = g_BriefingTexts;
    g_BriefingTexts = arg0;
}


/*
* Address: 0x7f004fb8
*/
void objectiveAdd(struct objective_entry* objective)
{
    g_Objectives[objective->menu] = objective;

    if (g_ObjectiveLastIndex < objective->menu)
    {
        g_ObjectiveLastIndex = objective->menu;
    }
}

void objectiveAddRoomEntered(struct criteria_roomentered *arg0)
{
    arg0->next = g_RoomEnteredCriteria;
    g_RoomEnteredCriteria = arg0;
}

void objectiveAddDeposit(struct criteria_deposit *arg0)
{
    arg0->next = g_DepositCriteria;
    g_DepositCriteria = arg0;
}


void objectiveAddPicture(struct criteria_picture *arg0)
{
    arg0->next = g_PictureCriteria;
    g_PictureCriteria = arg0;
}


//Public variables - move to header
// bss
//CODE.bss:80075D30
struct objective_entry * g_Objectives[OBJECTIVES_MAX];
OBJECTIVESTATUS         g_ObjectiveStatuses[OBJECTIVES_MAX]; //This is an array of 10 OBJECTIVESTATUS,

//CODE.bss:80075D80
u32 *g_Tags;
//CODE.bss:80075D84
struct watchMenuObjectiveText *g_BriefingTexts;
//CODE.bss:80075D88
struct criteria_roomentered *g_RoomEnteredCriteria;
//CODE.bss:80075D8C
struct criteria_deposit *g_DepositCriteria;
//CODE.bss:80075D90
struct criteria_picture *g_PictureCriteria;

// data
s32 g_ObjectiveLastIndex = 0xFFFFFFFF;
s32 g_ObjectiveStatusDisplayDisabled = FALSE;



/*
 * Return Tag with TagID
 */
TagObjectRecord *tagFind(s32 TagID) //#MATCH
{
    u16              ID  = TagID;
    TagObjectRecord *tag = g_Tags;
    while (tag)
    {
        if (tag->ID == ID)
        {
            return tag;
        }
        tag = tag->NextTag;
    }
    return NULL;
}





/*
 * Return Object with TagID  
 * Address 7F0570C0
 */
ObjectRecord *objFindByTagId(s32 TagID) //#MATCH
{
    TagObjectRecord *tag = tagFind(TagID);
    ObjectRecord *   obj = NULL;

    if (tag)
    {
        obj = tag->TaggedObject;
    }
    if (obj && !(obj->runtime_bitflags & 0x10))
    {
        obj = NULL; //clear object
    }
    return obj;
}



/*
* Address: 0x7f057104
*/
u8 * briefingGetText(WATCH_BRIEFING_PAGE page)
{
    struct watchMenuObjectiveText * curentry;
    u8 * textptr;

    textptr = NULL;

    for (curentry = g_BriefingTexts; curentry != 0; curentry = curentry->nextentry)
    {
        if (page == curentry->menu)
        {
            textptr = langGet(curentry->text);
            break;
        }
    }

    if (textptr == NULL)
    {
        if (page == 0)
        {
            textptr = langGet(getStringID(LMISC, MISC_STR_29_ERROR_LF)); //"E R R O R\n"
        }
        else if (page == 1)
        {
            textptr = langGet(getStringID(LMISC, MISC_STR_2A_NOBRIEFING_LF)); //"no briefing for this mission\n"
        }
        else
        {
            textptr = langGet(getStringID(LMISC, MISC_STR_2B_LF)); //"\n"
        }
    }

    return textptr;
}



//objectiveGetCount
s32 objectiveGetCount(void)
{
    return g_ObjectiveLastIndex + 1;
}



u8 * objectiveGetText(int objectiveIndex)
{
    
    if ((objectiveIndex < 10) && (g_Objectives[objectiveIndex] != 0)) {
        return langGet(g_Objectives[objectiveIndex]->text);
    }
    return 0;
}



s32 objectiveGetDifficulty(s32 objectiveIndex)
{
    struct objective_entry * entry;

    if (objectiveIndex < OBJECTIVES_MAX)
    {
        entry = g_Objectives[objectiveIndex];
        if (entry != NULL)
        {
            return entry->difficulty;
        }
    }
    return 0;
}



//horrible hack to get ai matching, but it does correctly refrence this func with 2 params
#pragma weak    objectiveGetStatus_WEAK = objectiveCheckStatus

/*
 * Return Status of objective.
 * Note: This matches but may not in future depending on the result of var
 * "OBJECTIVESTATUS *g_ObjectiveStatuses". 
 * https://decomp.me/scratch/MF31e
 */
OBJECTIVESTATUS objectiveCheckStatus(s32 objectiveNum) //#MATCH
{
    MissionObjectiveRecord *objective;
    OBJECTIVESTATUS         currentstatus;
    OBJECTIVESTATUS         status = OBJECTIVESTATUS_COMPLETE;

    if (objectiveNum < 10)
    {
        if (!&g_Objectives[objectiveNum]->id)
        {
            status = g_ObjectiveStatuses[objectiveNum];
        }
        else
        {
            // for each objective in objectives
            for (objective = &g_Objectives[objectiveNum]->id; objective->type != PROPDEF_OBJECTIVE_END; objective = sizepropdef(objective) + (PropDefHeaderRecord *)objective)
            {
                {
                    currentstatus = OBJECTIVESTATUS_COMPLETE;
                    switch (objective->type)
                    {
                        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
                        {
                            ObjectRecord *obj = objFindByTagId(objective->ObjRefID);
                            if (obj && obj->prop && objIsHealthy(obj))
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
                        {
                            if (!chrHasStageFlag(NULL, objective->ObjRefID))
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_FAIL_CONDITION:
                        {
                            if (chrHasStageFlag(NULL, objective->ObjRefID))
                            {
                                currentstatus = OBJECTIVESTATUS_FAILED;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
                        {
                            ObjectRecord *obj = objFindByTagId(objective->ObjRefID);
                            if (!obj || !obj->prop || !objIsHealthy(obj))
                            {
                                currentstatus = OBJECTIVESTATUS_FAILED;
                            }
                            else if (!invAnyPlayerHasProp(obj->prop))
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
                        {
                            ObjectRecord *obj = objFindByTagId(objective->ObjRefID);
                            if (obj && obj->prop && invAnyPlayerHasProp(obj->prop))
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
                        {
                            ObjectRecord *obj = objFindByTagId(objective->ObjRefID);
                            if (!objective->TextID) //0x8
                            {
                                if (!obj || !obj->prop || !objIsHealthy(obj))
                                {
                                    currentstatus = OBJECTIVESTATUS_FAILED;
                                }
                                else
                                {
                                    currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                                }
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_ENTER_ROOM:
                        {
                            if (!objective->TextID) //0x8
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
                        {
                            if (!objective->MinDificulty) //0xc
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_COPY_ITEM:
                        {
                            if (!playerHasCopiedGEKey())
                            {
                                currentstatus = OBJECTIVESTATUS_INCOMPLETE;
                            }
                            break;
                        }
                        case PROPDEF_OBJECTIVE_END:
                        case PROPDEF_OBJECTIVE_START:
                        case PROPDEF_OBJECTIVE_NULL:
                        {
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                    if (status == OBJECTIVESTATUS_COMPLETE)
                    {
                        if (currentstatus != OBJECTIVESTATUS_COMPLETE)
                        {
                            status = currentstatus;
                        }
                    }
                    else if ((status == OBJECTIVESTATUS_INCOMPLETE) && (currentstatus == OBJECTIVESTATUS_FAILED))
                    {
                        status = currentstatus;
                    }

                    //objective  = objheader;
                }
            }
        } // if objheader
    }     //objnum < 10

    return status;
}



bool objectivesAllComplete(void)
{
    DIFFICULTY objdiff;
    DIFFICULTY curdiff;
    int objective;
    
    for (objective = 0; objective < objectiveGetCount(); objective++)
    {
        objdiff = objectiveGetDifficulty(objective);
        curdiff = lvGetDifficulty();
        if ((objdiff <= curdiff) && (objectiveCheckStatus(objective) != OBJECTIVESTATUS_COMPLETE))
        {
            return FALSE;
        }
    }
    return TRUE;
}


void objectivesDisableStatusDisplay(void)
{
  g_ObjectiveStatusDisplayDisabled = TRUE;
  return;
}


void objectivesTickStatusMessages(void) 
{
    s32 i;
    s32 availableindex = 0;
    char buffer[50] = "";
    enum OBJECTIVESTATUS status;

    if (g_ObjectiveStatusDisplayDisabled) { return; }

    for (i = 0; i <= g_ObjectiveLastIndex; i++) {
        status = objectiveCheckStatus(i);
        if (g_ObjectiveStatuses[i] != status) {
            g_ObjectiveStatuses[i] = status;
            if (objectiveGetDifficulty(i) <= lvGetDifficulty()) {
                sprintf(&buffer, "%s %c: ", langGet(getStringID(LMISC, MISC_STR_2C_OBJECTIVE)), availableindex + 'a');
                if (status == OBJECTIVESTATUS_COMPLETE)
                {
                    strcat(&buffer, langGet(getStringID(LMISC, MISC_STR_2D_COMPLETED_LF)));
                }
                else if (status == OBJECTIVESTATUS_INCOMPLETE)
                {
                    strcat(&buffer, langGet(getStringID(LMISC, MISC_STR_2E_INCOMPLETE_LF)));
                }
                else if (status == OBJECTIVESTATUS_FAILED)
                {
                    strcat(&buffer, langGet(getStringID(LMISC, MISC_STR_2F_FAILED_LF)));
                }
                if (g_CoopMode)
                {
                    // Co-op: show it on every player's screen.
                    s32 prevplayer = playerGetCurrentNum();
                    s32 p;

                    for (p = 0; p < getPlayerCount(); p++)
                    {
                        playerSetCurrent(p);
#ifdef VERSION_US
                        hudmsgBottomShow(&buffer);
#else
                        hudmsgBottomShowText(&buffer);
#endif
                    }

                    playerSetCurrent(prevplayer);
                }
                else
                {
#ifdef VERSION_US
                    hudmsgBottomShow(&buffer);
#else
                    hudmsgBottomShowText(&buffer);
#endif
                }
            }
        }
        if (objectiveGetDifficulty(i) <= lvGetDifficulty()) {
            availableindex++;
        }
    }
}


/**
 * Iterate list of g_RoomEnteredCriteria, if it's the same
 * room as the argument then set the `flag` property.
 * 
 * US address 7F057744.
 * 
 * Perfect Dark method objectiveCheckRoomEntered.
*/
void objectivesCheckRoomEntered(s32 roomid)
{
    struct criteria_roomentered *var_v0;
    struct PadRecord* setupPad;
    struct StandTile *stan;
    
    var_v0 = g_RoomEnteredCriteria;
    while (var_v0 != NULL)
    {
        if (var_v0->status == 0)
        {
            if (isNotBoundPad(var_v0->pad))
            {
                setupPad = &g_CurrentSetup.pads[var_v0->pad];
            }
            else
            {
                setupPad = (struct PadRecord*)&g_CurrentSetup.boundpads[getBoundPadNum((s32)var_v0->pad)];
            }

            stan = setupPad->stan;

            if ((stan != NULL) && (roomid == stan->room))
            {
                var_v0->status = 1;
            }
        }

        var_v0 = var_v0->next;
    }
}





/**
 * US address 7F0577E8.
*/
void objectivesCheckDeposit(s32 weaponnum, s32 roomid)
{
    struct criteria_deposit *dep;
    s32 padid;
    PadRecord *pad;

    for (dep = g_DepositCriteria; dep != NULL; dep = dep->next)
    {
        if (dep->flag == 0 && weaponnum == dep->weaponnum)
        {
            padid = dep->padid;

            if (isNotBoundPad((u32)padid))
            {
                pad = &g_CurrentSetup.pads[padid];
            }
            else
            {
                pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padid)];
            }

            if (pad->stan != NULL && roomid == pad->stan->room)
            {
                dep->flag = 1;
            }
        }
    }
}



/**
 * NTSC address 0x7F057898.
*/
void objectivesCheckPicture(void)
{
    ObjectRecord *target_object;
    PropRecord *target_prop;
    struct coord3d sp84;
    struct coord2d sp7C;
    struct coord2d sp74;
    struct rectbbox sp64;
    struct criteria_picture *criteria;

    criteria = g_PictureCriteria;
    for (; criteria != NULL; criteria = criteria->next)
    {
        if (criteria->flag == 0)
        {
            target_object = objFindByTagId(criteria->tag_id);
            
            if (target_object != NULL)
            {
                target_prop = target_object->prop;
                
                if ((target_prop != NULL) 
                    && (target_prop->flags & PROPFLAG_ONSCREEN)
                    && (target_prop->zDepth >= 0.0f) // draw/render distance
                    && (objIsHealthy(target_object) != 0)
                    && (objGetOnscreenRenderBounds(target_object->prop, &sp84, &sp7C, &sp74) != 0))
                {
                    projectRectCornersTo2D(&sp84, &sp7C, &sp74, &sp64.right, &sp64.left);
                    
                    if (camGetScreenLeft() < sp64.right)
                    {
                        if ((sp64.right < (camGetScreenLeft() + camGetScreenWidth())) && (camGetScreenLeft() < sp64.left))
                        {
                            if ((sp64.left < (camGetScreenLeft() + camGetScreenWidth())) && (camGetScreenTop() < sp64.down))
                            {
                                if ((sp64.down < (camGetScreenTop() + camGetScreenHeight())) && (camGetScreenTop() < sp64.up))
                                {
                                    if (sp64.up < (camGetScreenTop() + camGetScreenHeight()))
                                    {
                                        criteria->flag = 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


//filebreak


void objectivesFinish(void)
{
    int i;

    for (i = 0; i < OBJECTIVES_MAX; i++)
    {
        if (g_Objectives[i] != NULL && (g_Objectives[i]->unkD & 1) && g_ObjectiveStatuses[i] != OBJECTIVESTATUS_FAILED)
        {
            g_ObjectiveStatuses[i] = OBJECTIVESTATUS_COMPLETE;
        }
    }
}
