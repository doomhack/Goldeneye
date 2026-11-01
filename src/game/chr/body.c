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

/**
 * Builds a chr's model from a body and (if the body has none of its own) a head,
 * loading their model files if needed. Used for Bond's model and for guards spawned
 * during play, not for guards set up at level load. Fills in model if given,
 * otherwise instantiates a new one. Without sunglasses the head's first switch
 * (its glasses) is hidden.
 */
struct Model *bodyCreateModel(s32 body, s32 head, struct ModelFileHeader *bodyHeader, struct ModelFileHeader *headHeader, s32 sunglasses, struct Model *model)
{
    f32 scale;
    f32 pov;
    ModelNode *headNode;
    ModelRwData_SwitchRecord *rwdata;

    scale = c_item_entries[body].scale * 0.10000001f;
    headNode = NULL;
    pov = c_item_entries[body].pov;

    if (cheatIsActive(CHEAT_DK_MODE))
    {
        scale *= 0.8f;
    }

    if (bodyHeader->RootNode == NULL)
    {
        modelFileLoadToPool(bodyHeader, c_item_entries[body].filename);
    }

    modelCalculateRwDataLen(bodyHeader);

    if (!c_item_entries[body].hasHead && (head >= 0))
    {
        headNode = bodyHeader->Switches[4]; // where the head attaches
        if (headNode != NULL)
        {
            if (headHeader->RootNode == NULL)
            {
                modelFileLoadToPool(headHeader, c_item_entries[head].filename);
            }

            modelCalculateRwDataLen(headHeader);

            bodyHeader->numRecords += headHeader->numRecords;
        }
    }

    if (model == NULL)
    {
        model = modelmgrInstantiateModelWithAnim(bodyHeader);
    }

    if (model != NULL)
    {
        modelSetScale(model, scale);
        modelSetAnimTranslationScale(model, pov);

        if ((headHeader != NULL) && !c_item_entries[body].hasHead)
        {
            bodyHeader->numRecords -= headHeader->numRecords;
            modelAttachHead(model, headNode, headHeader);

            if (!sunglasses && ((s32)headHeader->numSwitches > 0))
            {
                if (headHeader->Switches[0] != NULL)
                {
                    rwdata = &modelGetNodeRwData(model, headHeader->Switches[0])->Switch;
                    rwdata->visible = 0;
                }
            }
        }
    }

    return model;
}

Model *bodyCreateModelNew(s32 body, s32 head, ModelFileHeader *body_header, ModelFileHeader *head_header, s32 sunglasses)
{
    return bodyCreateModel(body, head, body_header, head_header, sunglasses, NULL);
}


/**
 * Builds a new model for a body and head. bitflags: 1 = sunglasses, 2 = sunglasses
 * half the time.
 */
Model *bodyCreateModelWithFlags(s32 body, s32 head, u32 bitflags)
{
    ModelFileHeader *body_header;
    ModelFileHeader *head_header;
    s32 sunglasses;

    body_header = c_item_entries[body].header;
    head_header = NULL;

    sunglasses = 0;

    if (bitflags & 1)
    {
        sunglasses = 1;
    }
    else if (bitflags & 2)
    {
        sunglasses = (randomGetNext() & 1) == 0;
    }

    if ((head >= 0) && !c_item_entries[body].hasHead)
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
 * A head for a body: for a male body one of the next four in the random male head
 * list, for a female body the current random female head.
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
 * A random head of the body's gender.
 */
s32 bodyGetRandomHead(s32 id)
{
    return (c_item_entries[id].isMale ? random_male_heads[randomGetNext() % num_male_heads] : random_female_heads[randomGetNext() % num_female_heads]);
}


void bodiesReset(void)
{
    s32 i;

    for (i = 0; c_item_entries[i].header != NULL; i++)
    {
        c_item_entries[i].header->RootNode = NULL;
    }

    current_random_male_head = randomGetNext() % num_male_heads;
    current_random_female_head = randomGetNext() % num_female_heads;
    current_random_body = randomGetNext() % num_bodies;
}
