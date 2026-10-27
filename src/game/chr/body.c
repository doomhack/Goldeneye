/**
 * Bodies and heads: loading their model files, building a chr's model from a body and a
 * head, and choosing random bodies and heads for guards. See docs/subsystems/guards.md.
 */
#include <ultra64.h>
#include <random.h>
#include <bondtypes.h>
#include "game/menu/cheat.h"
#include "game/chr/chr.h"
#include "game/chr/body.h"
#include "game/model/chrobjdata.h"
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/model/model.h"

// # This file is chrlv.c

/**
 * Address 0x7F0232E8 (VERSION_US)
 * Address 0x7F0235D8 (other version)
 * Not a very descriptive name for a function. What it does is load Bond's model or those
 * for spawning guards. It is not used for guards that spawn at level loading.
*/
struct Model *bodyCreateModel(s32 body, s32 head, struct ModelFileHeader *bodyHeader, struct ModelFileHeader *headHeader, s32 sunglasses, struct Model *model)
{
    f32 scale;
    f32 pov;
    s32 opcode;
    ModelRwData_SwitchRecord *rwdata;

    scale = c_item_entries[body].scale * 0.10000001f;
    opcode = 0;
    pov = c_item_entries[body].pov;

    if (cheatIsActive(CHEAT_DK_MODE))
    {
        scale *= 0.8f;
    }

    if (bodyHeader->RootNode == 0)
    {
        modelFileLoadToPool(bodyHeader, c_item_entries[body].filename);
    }

    modelCalculateRwDataLen(bodyHeader);

    if ((c_item_entries[body].hasHead == 0) && (head >= 0))
    {
        opcode = &bodyHeader->Switches[4]->Opcode;
        if (opcode != 0)
        {
            if (headHeader->RootNode == 0)
            {
                modelFileLoadToPool(headHeader, c_item_entries[head].filename);
            }

            modelCalculateRwDataLen(headHeader);

            bodyHeader->numRecords += headHeader->numRecords;
        }
    }

    if (model == 0)
    {
        model = modelmgrInstantiateModelWithAnim(bodyHeader);
    }

    if (model != 0)
    {
        modelSetScale(model, scale);
        modelSetAnimTranslationScale(model, pov);

        if ((headHeader != 0) && (c_item_entries[body].hasHead == 0))
        {
            bodyHeader->numRecords -= headHeader->numRecords;
            modelAttachHead(model, opcode, headHeader);

            if ((sunglasses == 0) && ((s32) headHeader->numSwitches > 0))
            {
                if (headHeader->Switches[0] != 0)
                {
                    rwdata = &modelGetNodeRwData(model, headHeader->Switches[0])->Switch;
                    rwdata->visible = 0;
                }
            }
        }
    }

    return model;
}

//sub_GAME_7F0234A8
Model *bodyCreateModelNew(int body,int head,ModelFileHeader *body_header, ModelFileHeader *head_header,int sunglasses)
{
  return bodyCreateModel(body,head,body_header,head_header,sunglasses,0x0);
}


/**
 * Address 0x7F0234D0.
 */
Model * bodyCreateModelWithFlags(s32 body, s32 head, u32 bitflags)
{
    ModelFileHeader *body_header;
    ModelFileHeader *head_header;
    s32 sunglasses;

    body_header = c_item_entries[body].header;
    head_header = NULL;

    sunglasses = 0;

    if ((bitflags & 1))
    {
        sunglasses = 1;
    }
    else if ((bitflags & 2))
    {
        sunglasses = (randomGetNext() & 1) == 0;
    }

    if ((head >= 0) && (c_item_entries[body].hasHead == 0))
    {
        head_header = c_item_entries[head].header;
    }

    return bodyCreateModelNew(body, head, body_header, head_header, sunglasses);
}

s32 bodyGetRandom(void)
{
  return list_of_bodies[current_random_body];
}

/**
 * Address 0x7F0235AC.
 * Get a Random Male Head Only
 * @param id: Integer Index of body
 * @return an integer ID of a head to use
 */
s32 bodyChooseHead(s32 id)
{
    s32 ret;

    if (c_item_entries[id].isMale)
    {
        ret = randomGetNext() & 3;
        ret = ((s32)current_random_male_head + ret) % (s32)num_male_heads;
        ret = random_male_heads[ret];
    }
    else
    {
        ret = random_female_heads[current_random_female_head];
    }

    return ret;
}

/**
 * Get a Random head for body ID
 * @param id: Integer Index of body
 * @return an integer ID of a head to use
*/
s32 bodyGetRandomHead(s32 id)
{
    return (c_item_entries[id].isMale ? random_male_heads[randomGetNext() % num_male_heads] : random_female_heads[randomGetNext() % num_female_heads]);
}


void bodiesReset(u32 stagenum)
{
    s32 i;
  
    for (i=0; c_item_entries[i].header!=0; i++)
    {
        c_item_entries[i].header->RootNode = (ModelNode*)0;
    }
    current_random_male_head = randomGetNext() % num_male_heads;
    current_random_female_head = randomGetNext() % num_female_heads;
    current_random_body = randomGetNext() % num_bodies;
}
