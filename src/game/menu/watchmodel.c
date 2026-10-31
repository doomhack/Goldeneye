/**
 * The 3D models in the pause watch: the held item and the controller
 * diagram on the controls page.
 */
#include "assets/obseg/text/LgunE.h"
#include "game/gfx/dyn.h"
#include "game/menu/watchmenu.h"
#include "game/model/model.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "joy.h"
#include "game/menu/watchmodel.h"

// Scratch for the controller's button positions while it is drawn on the watch
static u32 watchControllerButtonBases[48];


/**
 * Draws an item's model on the watch's inventory page.
 */
Gfx *watchRenderItem(Gfx *gdl, ITEM_IDS itemid, Mtxf *mtx, s32 alpha, s32 colour)
{
    ModelRenderData renderdata = {.zbufferenabled = TRUE, .flags = 3};
    Model model = {};
    u32 rwbuf[0x80 / sizeof(u32)];
    Mtxf tmp;
    Mtxf *matrices;
    union ModelRwData *rwdata;
    s32 i;
    s32 j;
    ModelFileHeader *bodymodel;

    if ((itemid == ITEM_TRIGGER) || (itemid == ITEM_WATCHLASER))
    {
        itemid = ITEM_WATCHMAGNETATTRACT;
    }

    gunSetWatchMenuWeapon(GUNRIGHT, itemid);

    if ((!gunHandModelIsEmpty(GUNRIGHT)) || (!gunGetHandModelItem(GUNRIGHT)))
    {
        return gdl;
    }

    bodymodel = &g_CurrentPlayer->handModelHeader[GUNRIGHT];

    if (!get_ptr_weapon_model_header_line(itemid))
    {
        goto earlyreturn;
    }

    if (weaponHasFlag(itemid, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_MENU))
    {
        goto earlyreturn;
    }

    matrices = dynAllocate(bodymodel->numMatrices << 6);
    
    for (i = 0; i < bodymodel->numMatrices; i++)
    {
        mtx4LoadIdentity(&matrices[i]);
    }

    i = 0;
    model.render_pos = (RenderPosView *)matrices;
    modelCalculateRwDataLen(bodymodel);
    modelInit(&model, bodymodel, rwbuf);
    gunSetFirstPersonPartsVisible(&model, 0);
    gunSetHeldItemVisible(&model, 1);

    if (bodymodel->Switches[1] != NULL)
    {
        rwdata = modelGetNodeRwData(&model, bodymodel->Switches[1]);

        if (rwdata != 0)
        {
            rwdata->Switch.visible = 0;
        }
    }

    mtx4Copy(mtx, matrices);
  
    if (bodymodel->Skeleton == (&skeleton_gun_revolver))
    {
        if (bodymodel->Switches[4] != NULL)
        {
            mtx4LoadTranslation((coord3d *) bodymodel->Switches[4]->Data, &tmp);
            mtx4Mult(mtx, &tmp, &matrices[3]);
        }
        if (bodymodel->Switches[5] != NULL)
        {
            mtx4LoadTranslation((coord3d *) bodymodel->Switches[5]->Data, &tmp);
            mtx4Mult(mtx, &tmp, &matrices[4]);
        }
    }

    if (bodymodel->Switches[6] != NULL)
    {
        coord3d *pos;
        s32 index;

        pos = (coord3d *) bodymodel->Switches[6]->Data;
        index = modelFindNodeMtxIndex(bodymodel->Switches[6], 0);
        mtx4LoadTranslation(pos, &tmp);
        mtx4Mult(mtx, &tmp, &matrices[index]);
    }

    if (bodymodel->Switches[7] != NULL)
    {
        coord3d *pos;
        s32 index;

        pos = (coord3d *) bodymodel->Switches[7]->Data;
        index = modelFindNodeMtxIndex(bodymodel->Switches[7], 0);
        mtx4LoadTranslation(pos, &tmp);
        mtx4Mult(mtx, &tmp, &matrices[index]);
    }

    if (bodymodel->numSwitches >= 19)
    {
        for (j = 0; j != 20; j += 4)
        {
            if ((*((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x48))) != NULL)
            {
                rwdata = modelGetNodeRwData(&model, *((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x48)));

                if (rwdata != NULL)
                {
                    rwdata->Switch.visible = 1;
                }
            }

            if ((*((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x5c))) != NULL)
            {
                rwdata = modelGetNodeRwData(&model, *((ModelNode **) ((((u8 *) bodymodel->Switches) + j) + 0x5c)));

                if (rwdata != NULL)
                {
                    rwdata->Switch.visible = 1;
                }
            }
        }
    }

    modelUpdateNodeRelations(&model);

    if ((((((itemid == ITEM_GOLDENGUN) || (itemid == ITEM_RUGER)) || (itemid == ITEM_KNIFE)) || (itemid == ITEM_THROWKNIFE)) || (itemid == ITEM_SILVERWPPK)) || (itemid == ITEM_GOLDWPPK))
    {
        gSPSetLights1(gdl++, g_WeaponEnvmapLight);
        gSPLookAt(gdl++, camGetLookAt());
    }

    if (bodymodel->numSwitches >= 17)
    {
        if (bodymodel->Switches[16] != NULL)
        {
            rwdata = modelGetNodeRwData(&model, bodymodel->Switches[17]);

            if (rwdata != 0)
            {
                rwdata->Switch.visible = 0;
            }
        }
    }

    renderdata.gdl = gdl;

    if (alpha >= 0xff)
    {
        renderdata.PropType = PROP_TYPE_WEAPON;
        renderdata.envcolour.word = colour;
    }
    else
    {
        renderdata.PropType = PROP_TYPE_PLAYER;
        renderdata.envcolour.word = alpha;
        renderdata.fogcolour.word = colour;
    }

    renderdata.zbufferenabled = FALSE;
    modelRender(&renderdata, &model);
    gdl = renderdata.gdl;
    mtxPushUnitWorldScale();
    j = 0;

    if (bodymodel->numMatrices > 0)
    {
        do
        {
            mtx4Copy((Mtxf *) (((u8 *) model.render_pos) + j), &tmp);
            mtxF2L(&tmp, (Mtxf *) ((i << 6) + (u8 *) model.render_pos));
            i++;
            j += 0x40;
        }
        while (i < bodymodel->numMatrices);
    }

    mtxPopWorldScale();

earlyreturn:
    return gdl;

}


/**
 * The matrix for one button of the watch menu's controller model:
 * out = m5 * translate(pos) * m3 * m2 * m1 * translate(base).
 */
void watchControllerButtonMtx(coord3d *base, Mtxf *m1, Mtxf *m2, Mtxf *m3, coord3d *pos, Mtxf *m5, Mtxf *out)
{
    Mtxf translation;

    mtx4LoadTranslation(base, out);
    mtx4MultInPlace(m1, out);
    mtx4MultInPlace(m2, out);
    mtx4MultInPlace(m3, out);
    mtx4LoadTranslation(pos, &translation);
    mtx4MultInPlace(&translation, out);
    mtx4MultInPlace(m5, out);
}


/**
 * Draws the controller on the watch's control page, with the buttons for
 * the current control style, and works out where each button label goes.
 */
Gfx* watchRenderController(Gfx* gdl, Mtxf* basemtx, s32 envcolour, bool animatebuttons, WatchContButtonPositions* buttonpositions, s8* contpadnum)
{
    ModelRenderData renderdata;
    Model modelstack = {};
    s32 i;
    f32 angle;
    u32 rwdata[26];
    Mtxf nodeMtx;
    Mtxf rotMtx;
    struct ModelFileHeader* objheader;
    coord3d* position;
    Mtxf* matrices;
    ModelNode* node;
    Mtxf buttonMtx;
    Mtxf transMtx;
    Mtxf tmpMtx;
    Mtxf pressMtx;
    Mtxf tiltMtx;
    Mtxf viewMtx;
    Mtxf identityMtx;
    coord3d coord_node2;
    coord3d coord_node11_pos;
    coord3d coord_node11_base;
    coord3d coord_node4_pos;
    coord3d coord_node4_base;
    coord3d coord_node5_pos;
    coord3d coord_node5_base;
    coord3d coord_node6_pos;
    coord3d coord_node6_base;
    coord3d coord_node7_pos;
    coord3d coord_node7_base;
    coord3d coord_node9_pos;
    coord3d coord_node9_base;
    coord3d coord_node8_pos;
    coord3d coord_node8_base;
    coord3d coord_node10_pos;
    coord3d coord_node10_base;
    coord3d coord_node3_pos;
    Mtxf stickMtx;
    coord3d coord_node3_base;
    coord3d coord_node1_pos;
    coord3d coord_node1_base;
    coord3d coord_node12_pos;
    coord3d coord_node12_base;

    renderdata = (ModelRenderData){.zbufferenabled = TRUE, .flags = 3};

    gunSetWatchMenuWeapon(GUNRIGHT, 0x55);

    if (!(gunHandModelIsEmpty(GUNRIGHT) && gunGetHandModelItem(GUNRIGHT)))
    {
        return gdl;
    }

    objheader = g_CurrentPlayer->handModelHeader;
    matrices = dynAllocate(objheader->numMatrices * (sizeof(Mtxf)));
    modelCalculateRwDataLen(objheader);


    modelInit(&modelstack, objheader, rwdata);
    modelstack.render_pos = (RenderPosView*) matrices;
    mtx4Copy(basemtx, &matrices[0]);
    
    for (i = 1; i < 13; i++)
    {
        position = &objheader->Switches[i]->Data->Group.Origin;

        // Update joy stick position and rotation.
        if (i == 2)
        {
            angle = ((((-(f32) joyGetStickX(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
            mtx4LoadZRotation(angle, &nodeMtx);
            angle = ((((-(f32) joyGetStickY(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
            mtx4LoadXRotation(angle, &rotMtx);
            mtx4MultInPlace(&rotMtx, &nodeMtx);
            mtx4SetTranslation(position, &nodeMtx);
        }
        else
        {
            mtx4LoadTranslation(position, &nodeMtx);
        }

        mtx4Mult(basemtx, &nodeMtx, &matrices[i]);
    }

    modelUpdateNodeRelations(&modelstack);
    renderdata.gdl = gdl;

    if (envcolour >= 0xff)
    {
        renderdata.PropType = PROP_TYPE_OBJ;
    }
    else
    {
        renderdata.PropType = PROP_TYPE_PLAYER;
        renderdata.envcolour.word = envcolour;
        renderdata.fogcolour.word = 0xFFFFFF00;
    }

    renderdata.zbufferenabled = TRUE;
    modelRender(&renderdata, &modelstack);
    gdl = renderdata.gdl;
    mtxPushUnitWorldScale();

    for (i = 0; i < objheader->numMatrices; i++)
    {
        mtx4Copy(&modelstack.render_pos[i].pos, &nodeMtx);
        mtxF2L(&nodeMtx, &modelstack.render_pos[i].pos);
    }

    mtxPopWorldScale();

    if (animatebuttons)
    {
        node = objheader->Switches[13];

        if (node)
        {
            modelGetNodeRwData(&modelstack, node)->Switch.visible = 0;
        }

        matrices = dynAllocate(objheader->numMatrices * (sizeof(Mtxf)));
        modelstack.render_pos = (RenderPosView*) matrices;

        // Update the positions and/or rotations of the free floating buttons on the sides of the screen.
        for (i = 0; i < 13; i++)
        {
            mtx4LoadLookAt(&viewMtx, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
            mtx4LoadLookAt(&buttonMtx, -5.0f, 2000.0f, -168.0f, -5.0f, 0.0f, -168.0f, 0.0f, 0.0f, -1.0f);
            mtx4LoadIdentity(&tiltMtx);
            mtx4LoadIdentity(&identityMtx);
            
            // Joy stick
            if (i == 2)
            {
                coord_node2 = buttonpositions->joystick;
                mtx4LoadTranslation(&coord_node2, &transMtx);
                angle = ((((-(f32) joyGetStickX(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
                mtx4LoadZRotation(angle, &nodeMtx);
                angle = ((((-(f32) joyGetStickY(*contpadnum)) * M_TAU_F) * 0.6f)) / 360.0f;
                mtx4LoadXRotation(angle, &rotMtx);
                mtx4MultInPlace(&rotMtx, &nodeMtx);
                mtx4MultInPlace(&transMtx, &nodeMtx);
                mtx4Mult(&viewMtx, &nodeMtx, &tmpMtx);
                mtx4Copy(&tmpMtx, &matrices[i]);
            }
            // R
            else if (i == 11)
            {
                coord_node11_base = ((coord3d *)watchControllerButtonBases)[5];
                coord_node11_pos = buttonpositions->r;

                if (joyGetButtons(*contpadnum, R_TRIG))
                {
                    mtx4LoadYRotation(-0.17453294f, &tiltMtx);
                }

                mtx4LoadXRotation(1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node11_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node11_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Up
            else if (i == 4)
            {
                coord_node4_base = ((coord3d *)watchControllerButtonBases)[6];
                coord_node4_pos = buttonpositions->cUp;

                if (joyGetButtons(*contpadnum, U_CBUTTONS))
                {
                    coord_node4_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node4_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node4_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Down
            else if (i == 5)
            {
                coord_node5_base = ((coord3d *)watchControllerButtonBases)[7];
                coord_node5_pos = buttonpositions->cDown;

                if (joyGetButtons(*contpadnum, D_CBUTTONS))
                {
                    coord_node5_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node5_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node5_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Left
            else if (i == 6)
            {
                coord_node6_base = ((coord3d *)watchControllerButtonBases)[8];
                coord_node6_pos = buttonpositions->cLeft;

                if (joyGetButtons(*contpadnum, L_CBUTTONS))
                {
                    coord_node6_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node6_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node6_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // C-Right
            else if (i == 7)
            {
                coord_node7_base = ((coord3d *)watchControllerButtonBases)[9];
                coord_node7_pos = buttonpositions->cRight;

                if (joyGetButtons(*contpadnum, R_CBUTTONS))
                {
                    coord_node7_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node7_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node7_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // B
            else if (i == 9)
            {
                coord_node9_base = ((coord3d *)watchControllerButtonBases)[10];
                coord_node9_pos = buttonpositions->b;

                if (joyGetButtons(*contpadnum, B_BUTTON))
                {
                    coord_node9_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976, &pressMtx);
                watchControllerButtonMtx(&coord_node9_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node9_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // A
            else if (i == 8)
            {
                coord_node8_base = ((coord3d *)watchControllerButtonBases)[11];
                coord_node8_pos = buttonpositions->a;

                if (joyGetButtons(*contpadnum, A_BUTTON))
                {
                    coord_node8_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node8_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node8_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // L
            else if (i == 10)
            {
                coord_node10_base = ((coord3d *)watchControllerButtonBases)[12];
                coord_node10_pos = buttonpositions->l;

                if (joyGetButtons(*contpadnum, L_TRIG))
                {
                    mtx4LoadYRotation(0.17453294f, &tiltMtx);
                }

                mtx4LoadXRotation(1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node10_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node10_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // D-pad
            else if (i == 3)
            {
                coord_node3_base = ((coord3d *)watchControllerButtonBases)[13];
                coord_node3_pos = buttonpositions->dPad;
                mtx4LoadIdentity(&stickMtx);

                if (joyGetButtons(*contpadnum, U_JPAD))
                {
                    mtx4LoadXRotation(-0.17453294f, &tiltMtx);
                }
                else if (joyGetButtons(*contpadnum, D_JPAD))
                {
                    mtx4LoadXRotation(0.17453294f, &tiltMtx);
                }

                if (joyGetButtons(*contpadnum, L_JPAD))
                {
                    mtx4LoadZRotation(0.17453294f, &stickMtx);
                }
                else if (joyGetButtons(*contpadnum, R_JPAD))
                {
                    mtx4LoadZRotation(-0.17453294f, &stickMtx);
                }

                mtx4MultInPlace(&stickMtx, &tiltMtx);
                mtx4LoadXRotation(-0.89759791f, &pressMtx);
                watchControllerButtonMtx(&coord_node3_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node3_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // Start
            else if (i == 1)
            {
                coord_node1_base = ((coord3d *)watchControllerButtonBases)[14];
                coord_node1_pos = buttonpositions->start;

                if (joyGetButtons(*contpadnum, START_BUTTON))
                {
                    coord_node1_base.y += -10.0f;
                }

                mtx4LoadXRotation(-1.0471976f, &pressMtx);
                watchControllerButtonMtx(&coord_node1_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node1_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            // Z
            else if (i == 12)
            {
                coord_node12_base = ((coord3d *)watchControllerButtonBases)[15];
                coord_node12_pos = buttonpositions->z;

                if (joyGetButtons(*contpadnum, Z_TRIG))
                {
                    mtx4LoadXRotation(-0.17453294f, &tiltMtx);
                }

                mtx4LoadZRotation(M_PI_F, &pressMtx);
                watchControllerButtonMtx(&coord_node12_base, &tiltMtx, &pressMtx, &identityMtx, &coord_node12_pos, &viewMtx, &buttonMtx);
                mtx4Copy(&buttonMtx, &matrices[i]);
            }
            else
            {
                mtx4Copy(basemtx, &matrices[i]);
            }
        }

        modelUpdateNodeRelations(&modelstack);
        renderdata.gdl = gdl;
        modelRender(&renderdata, &modelstack);
        gdl = renderdata.gdl;
        mtxPushUnitWorldScale();

        for (i = 0; i < objheader->numMatrices; i++)
        {
            mtx4Copy(&modelstack.render_pos[i].pos, &nodeMtx);
            mtxF2L(&nodeMtx, &modelstack.render_pos[i].pos);
        }

        mtxPopWorldScale();
    }

    return gdl;
}


/**
 * watchRenderController at full opacity.
 */
Gfx *watchRenderControllerOpaque(Gfx *gdl, Mtxf *basemtx, bool animatebuttons, WatchContButtonPositions *buttonpositions, s8 *contpadnum)
{
    return watchRenderController(gdl, basemtx, 0xff, animatebuttons, buttonpositions, contpadnum);
}
