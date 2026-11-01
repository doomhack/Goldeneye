/**
 * Bond's body as other players and the cameras see it: loading the body and
 * head models and the guns in its hands (playerLoadBody, playerBodyGiveGun), the
 * cuffs, posing it each frame to match the player (playerTick), and drawing it.
 */
#include "game/asset/asset.h"
#include "game/chr/body.h"
#include "game/chr/chraction.h"
#include "game/level/lv.h"
#include "game/menu/front.h"
#include "game/menu/watchmenu.h"
#include "game/model/model.h"
#include "game/model/modelfile.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/player/coop.h"
#include "game/prop/propobj.h"
#include "game/save/file.h"
#include "random.h"
#include "game/level/gamemode.h"

// Rounds down to a multiple of 64
#define ALIGN64_V3(val) (((val) | 0x3f) ^ 0x3f)

s32 g_bondviewBondDeathAnimations[] = {
    PTR_ANIM_death_forward_face_down,
    PTR_ANIM_death_forward_spin_face_up,
    PTR_ANIM_death_backward_fall_face_up1,
    PTR_ANIM_death_backward_spin_face_down_right,
    PTR_ANIM_death_backward_spin_face_up_right,
    PTR_ANIM_death_backward_spin_face_down_left,
    PTR_ANIM_death_backward_spin_face_up_left,
    PTR_ANIM_death_forward_face_down_hard,
    PTR_ANIM_death_forward_face_down_soft,
    PTR_ANIM_death_fetal_position_right,
    PTR_ANIM_death_fetal_position_left,
    PTR_ANIM_death_backward_fall_face_up2,
    0
};

s32 g_bondviewBondDeathAnimationsCount = 0;

static s32 g_PlayerTickCount = 0;

struct firing_anim_struct firing_animation_groups[][6] = {
    {{pistol_firing_animation_group1, 0, 0.1, 79.0, 87.0},
     {&g_ChrMoveFireAnims[2], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[3], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[9], 0, 0.5, 0.0, -1.0},
     {crouched_pistol_firing_animation_group1, 0, 0.1, 56.0, 68.0}},
    {{rifle_firing_animation_group1, 0, 0.050000001, 35.0, 40.0},
     {&g_ChrMoveFireAnims[0], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[1], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[9], 0, 0.5, 0.0, -1.0},
     {&crouched_rifle_firing_animation_group1[1], 0, 0.1, 45.0, 55.0}},
    {{NULL, 0x8194, 0.25, 0.0, -1.0},
     {NULL, 0x8204, 0.5, 0.0, -1.0},
     {NULL, 0x777C, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[9], 0, 0.5, 0.0, -1.0},
     {NULL, 0x6C18, 0.050000001, 28.0, 29.0}},
    {{doubles_firing_animation_group1, 0, 0.1, 32.0, 42.0},
     {&g_ChrMoveFireAnims[4], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[5], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {&g_ChrMoveFireAnims[8], 0, 0.5, 0.0, -1.0},
     {crouched_doubles_firing_animation_group1, 0, 0.1, 37.0, 47.0}}
 };


void playerLoadBody(void)
{
    f32                         yaw;
    ChrRecord                  *self;
    struct texpool              pool;
    ModelFileHeader            *bodyheader;
    ModelFileHeader            *headheader;
    ModelFileHeader            *pitemheader;
    u8                         *weaponbuf0;
    u8                         *weaponbuf1;
    s32                         cursor;
    s32                         size0;
    s32                         size1;
    s32                         helddst;
    struct player             **pp;
    s32                         prop;
    ITEM_IDS                    item;
    s32                         body;
    s32                         head;
    Model                      *model;

    yaw = bondviewGetPlayerYawRadians();
    if (g_CurrentPlayer->prop->chr == NULL)
    {
        weaponbuf0 = gunGetHandModelBuffer(GUNRIGHT);
        weaponbuf1 = gunGetHandModelBuffer(GUNLEFT);
        cursor     = 0;
        size0      = gunGetHandModelBufferSize(GUNRIGHT);
        size1      = gunGetHandModelBufferSize(GUNLEFT);
        item       = gunGetShownWeapon(GUNRIGHT);
        body       = BODY_Formal_Wear;
        head       = HEAD_Male_Brosnan_Default;
        model      = NULL;
        bodyheader = NULL;

        playerRemoveFromRooms(g_CurrentPlayer);

        if (counterOpIsGuardPlayer(playerGetCurrentNum()) && counterOpGetGuardModels(&head, &body))
        {
            // Counter-op: the guard player wears the guard they took over.
        }
        // Counter-op: until then (the intro) the guard player is a Bond, as
        // player 2's co-op character may be one with no head or body entry.
        else if (!gameIsMulti() && ((playerGetCurrentNum() == 0) || frontCoopPartnerIsBond() || counterOpIsGuardPlayer(playerGetCurrentNum())))
        {
            helddst = fileGetBondForCurrentFolder();
            switch (g_CurrentPlayer->bondtype)
            {
                case CUFF_BLUE:
                    break;

                case CUFF_BOILER:
                    body = BODY_Special_Operations_Uniform;
                    break;

                case CUFF_JUNGLE:
                    body = BODY_Jungle_Fatigues;
                    break;

                case CUFF_SNOW:
                    body = BODY_Parka;
                    break;

                case CUFF_BROSNAN:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_CONNERY:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_DALTON:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_MOORE:
                    body = BODY_Brosnan_Tuxedo;
                    break;

                case CUFF_FOLDER:
                    switch (helddst)
                    {
                        case BOND_BROSNAN:
                            body = BODY_Brosnan_Tuxedo;
                            break;

                        case BOND_CONNERY:
                            body = BODY_Brosnan_Tuxedo;
                            break;

                        case BOND_DALTON:
                            body = BODY_Brosnan_Tuxedo;
                            break;

                        case BOND_MOORE:
                            body = BODY_Brosnan_Tuxedo;
                            break;
                    }

                    break;
            }

            switch (helddst)
            {
                case BOND_BROSNAN:
                    switch (g_CurrentPlayer->bondtype)
                    {
                        case CUFF_BLUE:
                            break;

                        case CUFF_BOILER:
                            head = HEAD_Male_Brosnan_Boiler;
                            break;

                        case CUFF_JUNGLE:
                            head = HEAD_Male_Brosnan_Jungle;
                            break;

                        case CUFF_BROSNAN:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_CONNERY:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_DALTON:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_MOORE:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;

                        case CUFF_FOLDER:
                            head = HEAD_Male_Brosnan_Tuxedo;
                            break;
                    }

                    break;

                case BOND_CONNERY:
                    head = HEAD_Male_Brosnan_Tuxedo;
                    break;

                case BOND_DALTON:
                    head = HEAD_Male_Brosnan_Tuxedo;
                    break;

                case BOND_MOORE:
                    head = HEAD_Male_Brosnan_Tuxedo;
                    break;
            }
        }
        else if (gameIsTwoPlayerMission())
        {
            head = frontCoopGetPartnerHead();
            body = frontCoopGetPartnerBody();
        }
        else
        {
            head = frontGetMpCharHead(playerGetCurrentNum());
            body = frontGetMpCharBody(playerGetCurrentNum());
        }

        if (g_CameraMode == CAMERAMODE_SWIRL)
        {
            item = starting_weapon[GUNRIGHT];
        }

        if (getPlayerCount() == 1)
        {
            gunClearHandModel(GUNLEFT);
            gunClearHandModel(GUNRIGHT);
            texInitPool(&pool, weaponbuf1, size1);
            bodyheader  = gunGetHandModelHeader(GUNRIGHT);
            *bodyheader = *c_item_entries[body].header;
            modelFileLoad(bodyheader, c_item_entries[body].filename, weaponbuf0, size0, &pool);
            cursor = assetGetSizeByName(c_item_entries[body].filename);

            do
            {
                cursor      = ALIGN64_V3(cursor + 0x3f);
                headheader  = (ModelFileHeader *)(weaponbuf0 + cursor);
                cursor      = ALIGN64_V3(cursor + sizeof(ModelFileHeader) + 0x3f);
                *headheader = *c_item_entries[head].header;


                modelFileLoad(headheader, c_item_entries[head].filename, weaponbuf0 + cursor, size0 - cursor, &pool);
                cursor = ALIGN64_V3(assetGetSizeByName(c_item_entries[head].filename) + cursor + 0x3f);
                model  = (Model *)(weaponbuf0 + cursor);
                cursor = ALIGN64_V3(cursor + 0xfb);
                modelCalculateRwDataLen(bodyheader);
                modelCalculateRwDataLen(headheader);

                {
                    u32 *animdata;
                    s32  nrec;
                    animdata = (u32 *)(weaponbuf0 + cursor);
                    nrec     = (bodyheader->numRecords + headheader->numRecords) + 0xa;
                    cursor   = ALIGN64_V3(cursor + (nrec << 2) + 0x3f);
                    animInit(model, bodyheader, animdata);
                    model->rwdatalen = nrec;
                }

            } while (FALSE);
        }
        else
        {
            bodyheader = c_item_entries[body].header;

            if (bodyheader->RootNode == NULL)
            {
                modelFileLoadToPool(bodyheader, c_item_entries[body].filename);
            }
            if (c_item_entries[body].hasHead)
            {
                head       = -1;
                headheader = NULL;
            }
            else
            {
                headheader = c_item_entries[head].header;

                if (headheader->RootNode == NULL)
                {
                    modelFileLoadToPool(headheader, c_item_entries[head].filename);
                }
            }
        }

        g_CurrentPlayer->bodyModel = bodyCreateModel(body, head, bodyheader, headheader, 0, model);
        modelSetScale(g_CurrentPlayer->bodyModel, g_CurrentPlayer->bodyModel->scale * 0.97f);
        chrInit(g_CurrentPlayer->prop, g_CurrentPlayer->bodyModel, &g_CurrentPlayer->prop->pos, yaw, g_CurrentPlayer->prop->stan, NULL);
        pp = &g_CurrentPlayer;
        (*pp)->prop->type = PROP_TYPE_VIEWER;
        self              = (*pp)->prop->chr;
        self->chrflags |= CHRFLAG_INIT;
        modelSetRootPosition((*pp)->bodyModel, &(*pp)->prop->pos);
        modelSetRootRotY(g_CurrentPlayer->bodyModel, yaw);
        self->headnum = head;
        self->bodynum = body;
        prop = weaponGetChrModel(item);

        if (prop >= 0)
        {
            if (getPlayerCount() == 1)
            {
                helddst      = cursor;
                helddst      = ((s32)weaponbuf0) + helddst;
                cursor       = ALIGN64_V3(cursor + 0xc7);
                pitemheader  = gunGetHandModelHeader(GUNLEFT);
                *pitemheader = *PitemZ_entries[prop].header;
                modelFileLoad(pitemheader, PitemZ_entries[prop].filename, weaponbuf0 + cursor, size0 - cursor, &pool);
                assetGetSizeByName(PitemZ_entries[prop].filename);
                modelCalculateRwDataLen(pitemheader);
            }
            else
            {
                helddst     = 0;
                pitemheader = NULL;
            }

            chrGiveWeaponEx(self, prop, item, 0, (WeaponObjRecord *)helddst, pitemheader);
        }

        chrStandImmediate(self, 0.0f);
    }
    else
    {
        self = g_CurrentPlayer->prop->chr;

        if (self->model->anim != NULL)
        {
            return;
        }

        self->chrflags |= CHRFLAG_INIT;
        chrStandImmediate(self, 0.0f);
        modelSetRootPosition(g_CurrentPlayer->bodyModel, &g_CurrentPlayer->prop->pos);
        modelSetRootRotY(g_CurrentPlayer->bodyModel, yaw);
    }
}


void playerRemoveBody(void)
{
    if ((g_CurrentPlayer->prop->chr) && (getPlayerCount() == 1))
    {
        chrFreeResources(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->chr = NULL;
        g_CurrentPlayer->bodyModel = 0;
        g_CurrentPlayer->forceDisarmTicks = 1;
        playerUpdateRooms(g_CurrentPlayer);
    }
}


void playerSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex)
{
    s32 local;
    ModelNode **switches;
    ModelNode **base;
    ModelNode *node;
    s32 offset;
    s32 *rwdata;
    s32 index;
    s32 visible;

    local = fileGetBondForCurrentFolder();
    switches = header->Switches;
    offset = switchindex << 2;


    // byte-indexed on purpose: offset = switchindex * 4. &switches[i] won't match.
    base = (ModelNode **) (((u8 *) switches) + offset);

    if (base[0] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, base[0]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_BOILER;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 1;

    if (((void *) (index * 0)) != base[1])
    {
        node = switches[index];
        rwdata = (s32 *) modelGetNodeRwData(model, node);
        visible = g_CurrentPlayer->bondtype == CUFF_BROSNAN;
        if (visible == 0)
        {
            visible = g_CurrentPlayer->bondtype == CUFF_DALTON;
            if (visible == 0)
            {
                visible = g_CurrentPlayer->bondtype == CUFF_MOORE;
                if (visible == 0)
                {
                    visible = g_CurrentPlayer->bondtype == CUFF_FOLDER;
                    if (visible != 0)
                    {
                        visible = local != 1;
                    }
                }
            }
        }
        *rwdata = visible;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 2;

    if (base[2] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        visible = g_CurrentPlayer->bondtype == CUFF_CONNERY;

        if (visible == 0)
        {
            visible = g_CurrentPlayer->bondtype == CUFF_FOLDER;
            if (visible != 0)
            {
                visible = local == 1;
            }
        }

        *rwdata = visible;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 3;

    if (base[3] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_BLUE;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = (switchindex + 4) ^ (((switchindex + 4) ^ 0) * 0);

    if (base[4])
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_JUNGLE;
        switches = header->Switches;
        base = (ModelNode **) (((u8 *) switches) + offset);
    }

    index = switchindex + 5;

    if (base[5] != NULL)
    {
        rwdata = (s32 *) modelGetNodeRwData(model, switches[index]);
        *rwdata = g_CurrentPlayer->bondtype == CUFF_SNOW;
    }
}


s32 playerTick(PropRecord *prop)
{
    s32 index;
    ChrRecord *chr;
    s32 group;
    s32 ret;
    s32 sub;
    PropRecord *leftprop;
    f32 mtx[15];
    s32 tailret;
    s32 anim;
    f32 angle;
    f32 frame;
    f32 local90;
    f32 local8c;
    f32 local88;
    f32 fwd;
    f32 startframe;
    struct weapon_firing_animation_table *firingtable;
    coord3d off;
    s32 i;
    s32 found;
    s32 cur;
    PropRecord *rightprop;
    struct WeaponObjRecord *leftobj;
    struct WeaponObjRecord *rightobj;
    s32 setanim;
    struct player **ppointers;
 
    index = playerGetIndexForProp(prop);
    chr = prop->chr;
 
    if (chr != NULL)
    {
        if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
        {
            chr->hidden &= ~CHRHIDDEN_FREEZE;
        }
    }

    // Co-op: the partner sits out cutscenes and isn't shown in them, except
    // the level intro.
    if (g_CoopSoloViewIsCinema && (index != g_CoopSoloView) && !coopShowPartnerInCinema())
    {
        goto clear_and_return;
    }
 
    if (chr != NULL)
    {
        if ((g_playerPointers[index]->bodyModel != NULL) && (g_playerPointers[index]->cameramode == 1))
        {
            g_playerPointers[index]->perimEnabled = 0;
            ret = chrTick(prop);
            g_playerPointers[index]->perimEnabled = 1;
 
            g_playerPointers[index]->bond.collision_position.x = g_playerPointers[index]->prop->pos.x;
            g_playerPointers[index]->bond.collision_position.y = g_playerPointers[index]->prop->pos.y;
            g_playerPointers[index]->bond.collision_position.z = g_playerPointers[index]->prop->pos.z;
            g_playerPointers[index]->bond.current_tile_ptr = g_playerPointers[index]->prop->stan;
            playerUpdateRooms(g_playerPointers[index]);
 
            if (prop->flags & PROPFLAG_ONSCREEN)
            {
                RenderPosView *rp = g_playerPointers[index]->bodyModel->render_pos;
                mtx4MultAffine(camGetViewToWorldMtxf(), (Mtxf *) rp, (Mtxf *) mtx);
                g_playerPointers[index]->bond.pos.x = mtx[12] + (mtx[4] * 7.0f);
                g_playerPointers[index]->bond.pos.y = mtx[13] + (mtx[5] * 7.0f);
                g_playerPointers[index]->bond.pos.z = mtx[14] + (mtx[6] * 7.0f);
            }
 
            return ret;
        }
    }
 
    if (chr == NULL)
    {
        goto clear_and_return;
    }

    if (g_playerPointers[index]->bodyModel == NULL)
    {
        goto clear_and_return;
    }

    if (getPlayerCount() < 2)
    {
        goto clear_and_return;
    }

    if (playerGetCurrentNum() == index)
    {
        goto clear_and_return;
    }
 
    anim = 0;
    firingtable = NULL;
    local90 = -1.0f;
    frame = -1;
    leftprop = chrGetEquippedWeaponProp(chr, GUNLEFT);
    rightprop = chrGetEquippedWeaponProp(chr, GUNRIGHT);
    leftobj = NULL;
    rightobj = NULL;
    setanim = 0;
 
    if (leftprop != NULL)
    {
        leftobj = leftprop->weapon;
    }
    
    if (rightprop != NULL)
    {
        rightobj = rightprop->weapon;
    }
 
    ppointers = g_playerPointers;
 
    if (playerGetShuffledIndex(playerGetCurrentNum()) == 0)
    {
        g_PlayerTickCount = g_PlayerTickCount + 1;
    }
 
    /**
     * If the player count is 1 we jump to the bottom of the function with goto clear_and_return, so this block only applies to MP.
     * g_PlayerTickCount advances once for each remote player's prop ticked during the pass of the viewport whose player is first in the shuffle order.
     * That means g_PlayerTickCount reaches 2 after one frame of a 2 player game, and part way through the first frame of a 3 or 4 player game.
     * Nothing *ever* resets g_PlayerTickCount, so we're skipping this whole block only once per boot. The reason for doing this though
     * isn't quite clear so do chime in if you have any theories.
     */
    if (g_PlayerTickCount >= 2)
    {
        local8c = ppointers[index]->bodyAimPitch;
        local88 = ppointers[index]->bodyAimYaw;
 
        if (ppointers[index]->bonddead != FALSE)
        {
            found = 0;
 
            for (i = 0; i < g_bondviewBondDeathAnimationsCount; i++)
            {
                cur = ppointers[index]->players_cur_animation;
 
                if (cur == (g_bondviewBondDeathAnimations[i] + ((s32) g_AnimTable)))
                {
                    found = 1;
                }
            }
 
            if (found)
            {
                anim = ppointers[index]->players_cur_animation;
                angle = 0.5f;
            }
            else
            {
                anim = g_bondviewBondDeathAnimations[randomGetNext() % g_bondviewBondDeathAnimationsCount] + ((s32) g_AnimTable);
                angle = 0.5f;
            }
 
            cur = ppointers[index]->players_cur_animation;
            local8c = 0.0f;
            local88 = 0.0f;
            goto join_768;
        }
 
        if ((leftprop != NULL) && (rightprop != NULL))
        {
            group = 3;
        }
        else if ((leftprop == NULL) && (rightprop == NULL))
        {
            group = 2;
        }
        else if ((leftobj != NULL) && (weaponHasFlag(leftobj->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0))
        {
            group = 2;
        }
        else if ((rightobj != NULL) && (weaponHasFlag(rightobj->weaponnum, WEAPONSTATBITFLAG_HOLD_AS_GUN) == 0))
        {
            group = 2;
        }
        else if ((leftobj != NULL) && (weaponHasFlag(leftobj->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED) != 0))
        {
            group = 0;
        }
        else if ((rightobj != NULL) && (weaponHasFlag(rightobj->weaponnum, WEAPONSTATBITFLAG_ONLY_1_HANDED) != 0))
        {
            group = 0;
        }
        else
        {
            group = 1;
        }
 
        if (playerGetCrouchPos(index) == 1)
        {
            goto set_crouch_lean;
        }
        else if (playerGetCrouchPos(index) == 0)
        {
set_crouch_lean:
            angle = 1.0f;
            sub = 5;
        }
        else if ((ppointers[index]->speedsideways < 0.0f) && (firing_animation_groups[group][4].pointer != NULL))
        {
            sub = 4;
            angle = -ppointers[index]->speedsideways;
 
            if (ppointers[index]->bodyStrafeTurn < 90.0f)
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn + 15.0f;
            }
        }
        else if ((ppointers[index]->speedsideways > 0.0f) && (firing_animation_groups[group][3].pointer != NULL))
        {
            sub = 3;
            angle = ppointers[index]->speedsideways;
 
            if (ppointers[index]->bodyStrafeTurn > (-90.0f))
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn - 15.0f;
            }
        }
        else
        {
            frame = ppointers[index]->speedtheta;
 
            if (frame < 0.0f)
            {
                frame = -frame;
            }
 
            fwd = ppointers[index]->speedforwards;
 
            if (fwd < -0.050000001f)
            {
                frame = -frame;
 
                if (fwd < frame)
                {
                    frame = fwd;
                }
 
                if (ppointers[index]->headanim == 0)
                {
                    goto shared_double_neg;
                }
 
                sub = 2;
 
                if (-0.40000001f < fwd)
                {
shared_double_neg:
                    angle = frame + frame;
                    sub = 1;
 
                    if (angle < (-1.0f))
                    {
                        angle = -1.0f;
                    }
                }
                else
                {
                    angle = frame;
                    goto lean_return_to_centre;
                }
            }
            else
            {
                if (0.050000001f < fwd)
                {
                    goto shared_framefwd;
                }
                else if (0.050000001f < frame)
                {
shared_framefwd:
                    if (frame < fwd)
                    {
                        frame = fwd;
                    }
                }
                else
                {
                    goto set_full_lean;
                }
 
                if (ppointers[index]->headanim == 0)
                {
                    goto shared_double_pos;
                }
 
                sub = 2;
 
                if (fwd < 0.40000001f)
                {
shared_double_pos:
                    angle = frame + frame;
                    sub = 1;
 
                    if (1.0f < angle)
                    {
                        angle = 1.0f;
                    }
                }
                else
                {
                    angle = frame;
                    goto lean_return_to_centre;
                }
            }
 
            goto lean_return_to_centre;
 
set_full_lean:
            angle = 1.0f;
            sub = 0;
 
lean_return_to_centre:
            if (0.0f < ppointers[index]->bodyStrafeTurn)
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn - 15.0f;
            }
 
            if (ppointers[index]->bodyStrafeTurn < 0.0f)
            {
                ppointers[index]->bodyStrafeTurn = ppointers[index]->bodyStrafeTurn + 15.0f;
            }
        }
 
        {
            struct firing_anim_struct *fa = &firing_animation_groups[group][sub];
 
            firingtable = fa->pointer;
 
            if (fa->anim != 0)
            {
                anim = fa->anim + (s32) g_AnimTable;
            }
 
            angle *= fa->x;
            local90 = fa->z;
            frame = fa->y;
        }
 
        cur = ppointers[index]->players_cur_animation;
 
join_768:
        if ((firingtable != NULL) && (anim == 0))
        {
            anim = *((s32 *) firingtable);
        }
 
        if (anim != cur)
        {
            setanim = 1;
        }
 
        if (0.0f <= frame)
        {
            if ((ppointers[index]->bodyModel->animlooping == 0) || (frame != ppointers[index]->bodyModel->animloopframe))
            {
                setanim = 1;
            }
        }
 
        if (frame < 0.0f)
        {
            if (ppointers[index]->bodyModel->animlooping)
            {
                setanim = 1;
            }
        }
 
        if (setanim != 0)
        {
            if (ppointers[index]->bodyModel->anim2 == NULL)
            {
                startframe = (0.0f <= frame) ? (frame) : (0.0f);
                modelSetAnimation(ppointers[index]->bodyModel, (ModelAnimation *) anim, 0, startframe, angle, 16.0f);
                ppointers[index]->players_cur_animation = anim;
                ppointers[index]->bodyAnimSpeed = angle;
 
                if (0.0f <= frame)
                {
                    modelSetAnimLooping(ppointers[index]->bodyModel, frame, 16.0f);
                }
 
                if (0.0f <= local90)
                {
                    modelSetAnimEndFrame(ppointers[index]->bodyModel, local90);
                }
            }
 
            cur = ppointers[index]->players_cur_animation;
        }
        else
        {
            if (angle != ppointers[index]->bodyAnimSpeed)
            {
                modelSetAnimSpeed(ppointers[index]->bodyModel, angle, 1.0f);
                ppointers[index]->bodyAnimSpeed = angle;
            }
 
            cur = ppointers[index]->players_cur_animation;
        }
 
        if (anim == cur)
        {
            if (firingtable != NULL)
            {
                chr->hidden &= ~CHRHIDDEN_0400;
                chrUpdateAimendbackShoulders(chr, firingtable, 0, 1, local8c);
            }
            else
            {
                chr->aimendrshoulder = 0.0f;
                chr->aimendlshoulder = 0.0f;
                chr->hidden |= CHRHIDDEN_0400;
                chr->aimendback = local8c;
            }
        }
 
        chr->aimendsideback = local88;
        chr->aimendcount = 10;
    }
 
    prop->pos.x = ppointers[index]->bond.collision_position.x;
    prop->pos.y = ppointers[index]->bond.collision_position.y;
    prop->pos.z = ppointers[index]->bond.collision_position.z;
    prop->stan = ppointers[index]->bond.current_tile_ptr;
 
    modelGetRootPosition(chr->model, &off);
    off.x = prop->pos.x;
    off.z = prop->pos.z;
    modelSetRootPosition(chr->model, &off);
    modelSetRootRotY(chr->model, (((360.0f - ppointers[index]->vv_theta) + ppointers[index]->bodyStrafeTurn) * M_TAU_F) / 360.0f);
 
    chr->chrflags |= CHRFLAG_INIT;
    chr->actiontype = ACT_BONDMULTI;
    chr->act_bondmulti.animfloats = firingtable;
 
    chrSetFiring(chr, GUNRIGHT, ppointers[index]->hands[GUNRIGHT].firing);
    chrSetFiring(chr, GUNLEFT, ppointers[index]->hands[GUNLEFT].firing);
 
    tailret = chrTick(prop);
 
    for (i = 0; i != 2; i++)
    {
        if (chrGetHandPos(chr, i, &ppointers[index]->bodyHandPos[i]) != 0)
        {
            ppointers[index]->bodyHandPosTick[i] = g_LvTickCount;
        }
        else if (ppointers[index]->bodyHandPosTick[i] < (g_LvTickCount - 1))
        {
            ppointers[index]->bodyHandPos[i].x = ppointers[index]->hands[i].muzzlePos.x;
            ppointers[index]->bodyHandPos[i].y = ppointers[index]->hands[i].muzzlePos.y;
            ppointers[index]->bodyHandPos[i].z = ppointers[index]->hands[i].muzzlePos.z;
        }
    }
 
    chr->hidden |= CHRHIDDEN_FREEZE;
 
    prop->pos.x = ppointers[index]->bond.collision_position.x;
    prop->pos.y = ppointers[index]->bond.collision_position.y;
    prop->pos.z = ppointers[index]->bond.collision_position.z;
    prop->stan = ppointers[index]->bond.current_tile_ptr;
 
    return tailret;
 
clear_and_return:
    prop->flags &= ~PROPFLAG_ONSCREEN;

    return TICKOP_NONE;
}


Gfx *playerRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)
{
    ChrRecord *chr = prop->chr;

    if (chr != NULL)
    {
        // Co-op: players are hidden from each other while they overlap at a
        // shared spawn, then fade in (coopPartnerVisibility)
        u8 fadealpha = chr->fadealpha;

        chr->fadealpha = (u8)(fadealpha * coopPartnerVisibility());
        gdl = chrRenderProp(prop, gdl, withalpha);
        chr->fadealpha = fadealpha;
    }

    return gdl;
}


PROP weaponGetChrModel(ITEM_IDS item)
{
    PROP ret = -1;
    switch (item)
    {
        case ITEM_KNIFE:         ret = PROP_CHRKNIFE;         break; /* weapon_multi_hunting_knife */
        case ITEM_WPPK:          ret = PROP_CHRWPPK;          break; /* weapon_multi_pp7 */
        case ITEM_SKORPION:      ret = PROP_CHRSKORPION;      break; /* weapon_multi_klobb */
        case ITEM_AK47:          ret = PROP_CHRKALASH;        break; /* weapon_multi_kf7 */
        case ITEM_UZI:           ret = PROP_CHRUZI;           break; /* weapon_multi_zmg */
        case ITEM_MP5K:          ret = PROP_CHRMP5K;          break; /* weapon_multi_d5k */
        case ITEM_SPECTRE:       ret = PROP_CHRSPECTRE;       break; /* weapon_multi_phantom */
        case ITEM_M16:           ret = PROP_CHRM16;           break; /* weapon_multi_ar33 */
        case ITEM_FNP90:         ret = PROP_CHRFNP90;         break; /* weapon_multi_rcp90 */
        case ITEM_SHOTGUN:       ret = PROP_CHRSHOTGUN;       break; /* weapon_multi_shotgun */
        case ITEM_GRENADELAUNCH: ret = PROP_CHRGRENADELAUNCH; break; /* weapon_multi_grenade_laun */
        case ITEM_GRENADE:       ret = PROP_CHRGRENADE;       break; /* weapon_multi_hand */
        case ITEM_REMOTEMINE:    ret = PROP_CHRREMOTEMINE;    break; /* weapon_multi_remote */
        case ITEM_PROXIMITYMINE: ret = PROP_CHRPROXIMITYMINE; break; /* weapon_multi_prox */
        case ITEM_TIMEDMINE:     ret = PROP_CHRTIMEDMINE;     break; /* weapon_multi_timed */
        case ITEM_RUGER:         ret = PROP_CHRRUGER;         break; /* weapon_multi_cougar */
        case ITEM_LASER:         ret = PROP_CHRLASER;         break; /* weapon_multi_moonraker */
        case ITEM_SNIPERRIFLE:   ret = PROP_CHRSNIPERRIFLE;   break; /* weapon_multi_sniper */
        case ITEM_MP5KSIL:       ret = PROP_CHRMP5KSIL;       break; /* weapon_multi_d5k_silent */
        case ITEM_TT33:          ret = PROP_CHRTT33;          break; /* weapon_multi_dd44 */
        case ITEM_WPPKSIL:       ret = PROP_CHRWPPKSIL;       break; /* weapon_multi_pp7_silent */
        case ITEM_THROWKNIFE:    ret = PROP_CHRTHROWKNIFE;    break; /* weapon_multi_throwing_knife */
        case ITEM_AUTOSHOT:      ret = PROP_CHRAUTOSHOT;      break; /* weapon_multi_auto_shot */
        case ITEM_ROCKETLAUNCH:  ret = PROP_CHRROCKETLAUNCH;  break; /* weapon_multi_rocket_launch */
        case ITEM_GOLDENGUN:     ret = PROP_CHRGOLDEN;        break; /* weapon_multi_goldengun */
        case ITEM_SILVERWPPK:    ret = PROP_CHRWPPK;          break; /* weapon_multi_pp7_special1 */
        case ITEM_GOLDWPPK:      ret = PROP_CHRWPPK;          break; /* weapon_multi_pp7_special2 */
        case ITEM_TOKEN:         ret = PROP_FLAG;             break;
        case ITEM_UNARMED:       break;
        case ITEM_TASER:         break;

        default:
            break;
    }
    return ret;
}


void playerBodyRemoveGun(enum GUNHAND hand)
{
    chrRemoveHeldWeapon(g_CurrentPlayer->prop->chr, hand);
}


void playerBodyGiveGun(enum GUNHAND hand)
{
    ChrRecord *chr;
    enum ITEM_IDS wepid;
    enum PROP prop;
    s32 flags;

    chr = g_CurrentPlayer->prop->chr;

    if (chr->weapons_held[hand] == NULL)
    {
        wepid = gunGetWeapon(hand);
        prop = weaponGetChrModel(wepid);
        if (prop >= 0)
        {
            flags = ((hand * 4) == 0)
                  ? 0
                  : PROPFLAG_WEAPON_LEFTHANDED;
            chrGiveWeaponEx(chr, prop, wepid, flags, NULL, NULL);
        }
    }
}
