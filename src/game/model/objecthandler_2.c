#include <ultra64.h>
#include "game/model/chrobjdata.h"
#include "game/gfx/image.h"
#include "game/math/trig.h"
#include "game/math/round.h"
#include "game/model/math_unk_05A9E0.h"
#include "game/model/model.h"
#include "game/asset/fileload.h"
#include <memp.h>
#include "game/model/objecthandler.h"
#include "game/math/quaternion.h"
#include "game/gfx/tex.h"


/***
 * Perfect Dark:
 * void modeldef0f1a7560(struct modeldef *modeldef, u16 filenum, u32 arg2, struct modeldef *modeldef2, struct texpool *texpool, bool arg5)
 * 
 * NTSC address 0x7F0762E0.
*/
void modelFileLoadTextures(ModelFileHeader *objheader, u8 *name, u8 *dst, struct texpool *buffer)
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

    filedata = (s32) objheader->Switches;
    filenum = fileGetIndex((char *) name);

    romremaining = fileGetAllocSize(filenum);
    pcremaining = fileGetLoadedSize(filenum);
    node = 0;
    modelIterateDisplayLists(objheader, &node, &gdl);

    if (gdl != 0)
    {
        name = (u8 *) ((pcremaining - ((s32) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)))) + ((s32) filedata));
        
        /* The signed lvalue cast is required for the compiler to choose the target registers. */
        replacementgdl = (u32)*(s32 *)&gdl;
        
        delta = ((s32) ((romremaining + filedata) - (s32) name)) - ((s32) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)));
        
        texCopyGdls((Gfx *) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)), (Gfx *) ((romremaining + filedata) - (s32) name), (s32) name);

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
                    name = (u8 *) (((s32) gdl) - ((s32) curgdl));
                }
                else
                {
                    name = (u8 *) ((((s32) (filedata + pcremaining)) - ((s32) objheader->Switches)) - (((u32) curgdl) & 0x00ffffff));
                }
                
                modelNodeReplaceGdl((u32) objheader, curnode, curgdl, (Gfx *) replacementgdl);
                
                replacementgdl += texLoadFromGdl( (Gfx *) ((((u8 *) objheader->Switches) + (((u32) curgdl) & 0x00ffffff)) + delta), (s32) name, (Gfx *) (((u8 *) objheader->Switches) + (replacementgdl & 0x00ffffff)), buffer);
            } 
            while (node != 0);
        }

        name = (u8 *) (((s32) (((u8 *) objheader->Switches) + (replacementgdl & 0x00ffffff))) - filedata);

        fileSetSize(filenum, (u8 *) filedata, (((s32) name + 0xf) & (~0xf)), dst == 0);
    }
}


/***
 * NTSC addres 0x7F0764A4.
*/
void modelFileLoad(struct ModelFileHeader *objheader, u8 *name, u8* dst, s32 size, struct texpool * buffer)
{
    void *filedata;

    if (dst != 0)
    {
        filedata = fileLoadByNameToAddr(name, FILELOAD_KEEP_SPARE, dst, size);
    }
    else
    {
        filedata = fileLoadByNameToPool(name, FILELOAD_KEEP_SPARE, 0x100, MEMPOOL_STAGE);
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


void modelFileLoadToAddr(struct ModelFileHeader *header,int *recallstring,int *targetloc,int sizeleft)
{
   modelFileLoad(header,recallstring,targetloc,sizeleft,0);
   return;
}





