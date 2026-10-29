/**
 * Loading model files (the c*, g* and p* files: characters, guns and props).
 *
 * A model file holds the node tree, the switch table and the texture list, with pointers
 * stored as offsets from 0x05000000. modelFileLoad loads the file into the stage pool (or a
 * given buffer), turns those offsets into real pointers, then loads the model's textures
 * into the space after the display lists and trims the allocation to fit.
 */
#include <ultra64.h>
#include "game/model/chrobjdata.h"
#include "game/gfx/texload.h"
#include "game/math/trig.h"
#include "game/math/round.h"
#include "game/model/model.h"
#include "game/model/model.h"
#include "game/asset/asset.h"
#include <memp.h>
#include "game/model/model.h"
#include "game/model/jointlist.h"
#include "game/model/modelfile.h"
#include "game/math/quaternion.h"
#include "game/gfx/tex.h"


/**
 * Loads the textures a model's display lists use, moving the display lists to the end of
 * the file's buffer first so the textures can go in the space after them. Then trims the
 * file's allocation (when it is in the stage pool) to what is used.
 */
void modelFileLoadTextures(ModelFileHeader *objheader, char *name, u8 *dst, struct texpool *buffer)
{
    ModelNode *node;
    s32 romremaining;
    Gfx *gdl;
    s32 pcremaining;
    u32 replacementgdl;
    ModelNode *curnode;
    Gfx *curgdl;
    s32 delta;
    s32 filedata;
    s32 filenum;
    s32 size; // bytes of display list data being processed

    filedata = (s32) objheader->Switches;
    filenum = assetFind(name);

    romremaining = assetGetBufferSize(filenum);
    pcremaining = assetGetSize(filenum);
    node = 0;
    modelIterateDisplayLists(objheader, &node, &gdl);

    if (gdl != 0)
    {
        size = (s32) ((pcremaining - ((s32) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)))) + ((s32) filedata));
        
        /* The signed lvalue cast is required for the compiler to choose the target registers. */
        replacementgdl = (u32)*(s32 *)&gdl;
        
        delta = ((s32) ((romremaining + filedata) - size)) - ((s32) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)));
        
        texCopyGdls((Gfx *) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)), (Gfx *) ((romremaining + filedata) - size), size);

        texLoadFromModelFileHeader(objheader, buffer);

        if (node != 0)
        {
            do
            {
                curnode = node;
                curgdl = gdl;
                modelIterateDisplayLists(objheader, &node, &gdl);
                
                if (gdl != 0)
                {
                    size = (s32) (((s32) gdl) - ((s32) curgdl));
                }
                else
                {
                    size = (s32) ((((s32) (filedata + pcremaining)) - ((s32) objheader->Switches)) - (((u32) curgdl) & 0x00ffffff));
                }
                
                modelNodeReplaceGdl(curnode, curgdl, (Gfx *) replacementgdl);
                
                replacementgdl += texLoadFromGdl( (Gfx *) ((((u8 *) objheader->Switches) + (((u32) curgdl) & 0x00ffffff)) + delta), size, (Gfx *) (((u8 *) objheader->Switches) + (replacementgdl & 0x00ffffff)), buffer);
            } 
            while (node != 0);
        }

        size = (s32) (((s32) (((u8 *) objheader->Switches) + (replacementgdl & 0x00ffffff))) - filedata);

        assetSetSize(filenum, (u8 *) filedata, ((size + 0xf) & (~0xf)), dst == 0);
    }
}


/**
 * Loads a model file into `dst` (`size` bytes), or into the stage pool if `dst` is NULL,
 * and gets it ready to use: header pointers set, offsets turned into pointers, textures
 * loaded.
 */
void modelFileLoad(struct ModelFileHeader *objheader, char *name, u8* dst, s32 size, struct texpool * buffer)
{
    void *filedata;

    if (dst != 0)
    {
        filedata = assetLoadInto(name, dst, size);
    }
    else
    {
        filedata = assetLoadUntrimmed(name, MEMPOOL_STAGE);
    }
    
    objheader->Switches = (struct ModelNode **)filedata;
    
    // hmmmmmmmmmmmm
    objheader->Textures = (struct ModelFileTextures *)&((s32*)filedata)[objheader->numSwitches];
    
    objheader->RootNode = (struct ModelNode *)&objheader->Textures[objheader->numtextures];
    
    modelFilePromotePointers(objheader, 0x5000000, filedata);
    modelFileLoadTextures(objheader, name, dst, buffer);
}


void modelFileLoadToPool(struct ModelFileHeader *header,char *name)
{
   modelFileLoad(header,name,0,0,0);
   return;
}


