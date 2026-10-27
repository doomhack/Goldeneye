
#include <ultra64.h>
#include "game/level/lv.h"
#include "game/menu/cheat.h"
#include "game/menu/front.h"
#include "game/replay/ramromreplay.h"
#include <rom.h>
#include <macro.h>
#include "game/save/file.h"
#include <random.h>
#include "joy.h"
#include "game/level/frametiming.h"
#include "game/replay/testhooks.h"

struct ramrom_struct
{
    ramromfilestructure *fdata;
    s32 locked;
};

struct ramrom_blockbuf
{
    s8 stick_x;
    s8 stick_y;
    u8 button_low;
    u8 button_high;
};
struct ramrom_seed
{
    u8 speedframes;
    u8 count;
    u8 randseed;
    u8 check;
};

//move me to better home
extern u32* ramrom_Dam_1;
extern u32* ramrom_Dam_2;
extern u32* ramrom_Facility_1;
extern u32* ramrom_Facility_2;
extern u32* ramrom_Facility_3;
extern u32* ramrom_Runway_1;
extern u32* ramrom_Runway_2;
extern u32* ramrom_BunkerI_1;
extern u32* ramrom_BunkerI_2;
extern u32* ramrom_Silo_1;
extern u32* ramrom_Silo_2;
extern u32* ramrom_Frigate_1;
extern u32* ramrom_Frigate_2;
extern u32* ramrom_Train;

struct ramrom_struct ramrom_table[] = {
    {&ramrom_Dam_1, 0},
    {&ramrom_Dam_2, 0},
    {&ramrom_Facility_1, 0},
    {&ramrom_Facility_2, 0},
    {&ramrom_Facility_3, 0},
    {&ramrom_Runway_1, 0},
    {&ramrom_Runway_2, 0},
    {&ramrom_BunkerI_1, 0},
    {&ramrom_BunkerI_2, 0},
    {&ramrom_Silo_1, 0},
    {&ramrom_Silo_2, 0},
    {&ramrom_Frigate_1, 0},
    {&ramrom_Frigate_2, 0},
    {&ramrom_Train, 0},
    {0,0}
};

ramromfilestructure* g_DemoFile = 0;
struct ramrom_seed * ramrom_blkbuf_2 = NULL;
struct ramrom_blockbuf * ramrom_blkbuf_3 = NULL;
s32 is_ramrom_flag = 0;
s32 ramrom_demo_related_3 = 0;
s32 g_ramromPlayBackFlag = 0;
s32 recording_ramrom_flag = 0;
s32 ramrom_demo_related_6 = 0;
s32 g_ramromRecordFlag = 0;
//                     .align 4





void ramromFadeToTitle(void);
s32 ramrom_replay_handler(struct contsample *arg0, s32 arg1);
void record_player_input_as_packet(struct contsample *arg0, s32 arg1, s32 arg2);
void copy_current_ingame_registers_before_ramrom_playback(ramromfilestructure *state);
void copy_recorded_ramrom_registers_to_proper_place_ingame(ramromfilestructure *state);


void clear_ramrom_block_buffer_heading_ptrs(void) {
    g_DemoFile = 0;
    ramrom_blkbuf_2 = 0;
    ramrom_blkbuf_3 = 0;
}


s32 get_is_ramrom_flag(void) {
    return is_ramrom_flag;
}


s32 get_recording_ramrom_flag(void) {
    return recording_ramrom_flag;
}


s32 frontRunStageTick(void) {
    return g_ramromPlayBackFlag;
}


void finalize_ramrom_on_hw(void)
{
    u8 buffer[0x28];
    u8 *p;
    void *a1;

    p = ALIGN16_a((s32)buffer);
    p[0] = 0;
    p[1] = 0;

    romWrite((void *) p, (void *) address_demo_loaded, 0x10U);
    
    address_demo_loaded += 4;

    a1 = INDY_RAMROM_DEMO_POINTER;

    g_DemoFile = romCopyUnaligned(ramrom_data_target, a1, 0xf0);
    g_DemoFile->totaltime_ms = g_GlobalTimer - g_ClockTimer;
    g_DemoFile->filesize = (s32)address_demo_loaded - (s32)a1;
    romWrite(g_DemoFile, a1, 0xf0);
}


void record_player_input_as_packet(struct contsample *arg0, s32 arg1, s32 arg2)
{
    s32 temp_t5;
    s32 temp_t1;
    s32 var_a0;
    s32 var_a2;
    u8 var_a3;
    s32 var_t2;
    struct ramrom_blockbuf *temp_v0;
    s32 temp_s0;

    temp_t5 = ALIGN16_a((s32)&ramrom_data_target[0x1f8]);
    temp_t1 = g_DemoFile->size_cmds;

    var_t2 = 0;
    var_a3 = 0;
    
    ramrom_blkbuf_2 = (struct ramrom_seed *)temp_t5;
    ramrom_blkbuf_3 = (struct ramrom_blockbuf *)(ramrom_blkbuf_2 + 1);

    // loop structure based on: void joyConsumeSamples(struct contdata *contdata)
    if (arg1 != arg2)
    {
        var_a2 = (s32) (arg1 + 1) % CONTSAMPLE_LEN;
        while (1)
        {
            for (var_a0 = 0; var_a0 < temp_t1; var_a0++)
            {
                temp_v0 = ramrom_blkbuf_3 + (var_t2 * temp_t1) + var_a0;

                temp_v0->stick_x = arg0->pads[var_a2*4 + var_a0].stick_x;
                temp_v0->stick_y = arg0->pads[var_a2*4 + var_a0].stick_y;
                temp_v0->button_low = arg0->pads[var_a2*4 + var_a0].button & 0xFF;
                temp_v0->button_high = arg0->pads[var_a2*4 + var_a0].button >> 8;
    
                var_a3 += (u8) (
                    (u8)temp_v0->stick_x
                    + (u8)temp_v0->stick_y
                    + temp_v0->button_low
                    + temp_v0->button_high);
            }
            var_t2++;

            if (var_a2 == arg2)
            {
                break;
            }

            var_a2 = (s32) (var_a2 + 1) % CONTSAMPLE_LEN;
        }
    }
        
    ramrom_blkbuf_2->count = var_t2;
    ramrom_blkbuf_2->speedframes = g_FrameDelta;
    ramrom_blkbuf_2->randseed = g_randomSeed;
    
    var_a3 += (u8) ((u8)ramrom_blkbuf_2->speedframes + (u8)ramrom_blkbuf_2->count + ramrom_blkbuf_2->randseed);
    ramrom_blkbuf_2->check = var_a3;

    temp_s0 = (temp_t1 * 4 * var_t2) + sizeof(s32);

    romWrite((void *) ramrom_blkbuf_2, address_demo_loaded, ALIGN16_a(temp_s0));

    address_demo_loaded += align_addr_even(temp_s0 + 1);
}




s32 ramrom_replay_handler(struct contsample *arg0, s32 arg1)
{
    s32 var_a3;
    s32 var_a0;
    struct ramrom_blockbuf *temp_v0;
    u8 var_t0;
    s32 temp_a2;
    s32 temp_t2;

    var_t0 = 0;
    temp_a2 = (s32) g_DemoFile->size_cmds;
    temp_t2 = ramrom_blkbuf_2->count;

    for (var_a3 = 0; var_a3 < temp_t2; var_a3++)
    {
        arg1 = (s32) (arg1 + 1) % CONTSAMPLE_LEN;

        for (var_a0 = 0; var_a0 < MAXCONTROLLERS; var_a0++)
        {
            if (var_a0 < temp_a2)
            {
                temp_v0 = ramrom_blkbuf_3 + (var_a3 * temp_a2) + var_a0;

                arg0->pads[arg1 * 4 + var_a0].stick_x = temp_v0->stick_x;
                arg0->pads[arg1 * 4 + var_a0].stick_y = temp_v0->stick_y;
                arg0->pads[arg1 * 4 + var_a0].button = (temp_v0->button_high << 8) | temp_v0->button_low;

                var_t0 += (u8)((u8)temp_v0->stick_x + (u8)temp_v0->stick_y + temp_v0->button_low + temp_v0->button_high);
            }
            else
            {
                arg0->pads[arg1 * 4 + var_a0].stick_x = 0;
                arg0->pads[arg1 * 4 + var_a0].stick_y = 0;
                arg0->pads[arg1 * 4 + var_a0].button = 0;
            }
        }
    }

    if (ramrom_blkbuf_2->randseed != (u8)g_randomSeed)
    {
        // The game has drifted from the recording.
        g_TestDemoDesynced = TRUE;
        ramromFadeToTitle();
    }

    var_t0 += (u8)((u8)ramrom_blkbuf_2->speedframes + (u8)ramrom_blkbuf_2->count + ramrom_blkbuf_2->randseed);
    if (ramrom_blkbuf_2->check != var_t0)
    {
        g_TestDemoDesynced = TRUE;
        ramromFadeToTitle();
    }
    
    joySetContDataIndex(0);
    
    if (joyGetButtonsPressedThisFrame(0, 0xFFFFU) != 0)
    {
        ramromFadeToTitle();
        g_FrontSkipIntro = TRUE;
    }
    
    joySetContDataIndex(1);

    return arg1;
}



void iterate_ramrom_entries_handle_camera_out(void)
{
    s32 temp_v1;
    s32 var_a3;
    
    ramrom_blkbuf_2 = romCopyUnaligned(ramrom_data_target + 0x1F8, address_demo_loaded, sizeof(struct ramrom_seed));

    var_a3 = ramrom_blkbuf_2->count;
    if (var_a3 > 0)
    {
        ramrom_blkbuf_3 = romCopyUnaligned(
            ramrom_data_target + 0x21E,
            address_demo_loaded + 4,
            g_DemoFile->size_cmds * sizeof(struct ramrom_blockbuf) * ramrom_blkbuf_2->count);
    }

    var_a3 = ramrom_blkbuf_2->count;
    if (var_a3 == 0 && ramrom_blkbuf_2->speedframes == 0)
    {
        ramromFadeToTitle();
    }
    else
    {
        // 5 is ??
        address_demo_loaded += align_addr_even((g_DemoFile->size_cmds * sizeof(struct ramrom_blockbuf) * ramrom_blkbuf_2->count) + 5);
    }

    frameAdvance(ramrom_blkbuf_2->speedframes);

    // BUG? Does this need to be adjusted for PAL?
    temp_v1 = g_DemoFile->totaltime_ms - 0x3C;
    if ((g_GlobalTimer >= temp_v1) && ((g_GlobalTimer - g_ClockTimer) < temp_v1))
    {
        ramromFadeToTitle();
    }
}

void copy_current_ingame_registers_before_ramrom_playback(ramromfilestructure *state)
{
    state->randomseed = g_randomSeed;
    state->randomizer = g_chrObjRandomSeed;
    state->mode = g_FrontGameMode;
    state->numplayers = g_MpPlayerCount;
    state->scenario = g_MpScenario;
    state->mpstage_sel = g_MpStage;
    state->gamelength = g_MpGameLength;
    state->weaponSet = mpWeaponSetGetIndex();
    state->mp_char[0] = g_MpChar[0];
    state->mp_char[1] = g_MpChar[1];
    state->mp_char[2] = g_MpChar[2];
    state->mp_char[3] = g_MpChar[3];
    state->mp_handi[0] = g_MpHandicap[0];
    state->mp_handi[1] = g_MpHandicap[1];
    state->mp_handi[2] = g_MpHandicap[2];
    state->mp_handi[3] = g_MpHandicap[3];
    state->mp_contstyle[0] = g_MpControlStyle[0];
    state->mp_contstyle[1] = g_MpControlStyle[1];
    state->mp_contstyle[2] = g_MpControlStyle[2];
    state->mp_contstyle[3] = g_MpControlStyle[3];
    state->aim_option = g_MpAimSetting;
    state->mp_flags[0] = frontGetMpTeam(0);
    state->mp_flags[1] = frontGetMpTeam(1);
    state->mp_flags[2] = frontGetMpTeam(2);
    state->mp_flags[3] = frontGetMpTeam(3);
}

void copy_recorded_ramrom_registers_to_proper_place_ingame(ramromfilestructure *state)
{
    g_randomSeed = state->randomseed;
    g_chrObjRandomSeed = state->randomizer;
    g_FrontGameMode = state->mode;
    g_MpPlayerCount = state->numplayers;
    g_MpScenario = state->scenario;
    g_MpStage = state->mpstage_sel;
    g_MpGameLength = state->gamelength;
    mpWeaponSetSelect(state->weaponSet);
    g_MpChar[0] = state->mp_char[0];
    g_MpChar[1] = state->mp_char[1];
    g_MpChar[2] = state->mp_char[2];
    g_MpChar[3] = state->mp_char[3];
    g_MpHandicap[0] = state->mp_handi[0];
    g_MpHandicap[1] = state->mp_handi[1];
    g_MpHandicap[2] = state->mp_handi[2];
    g_MpHandicap[3] = state->mp_handi[3];
    g_MpControlStyle[0] = state->mp_contstyle[0];
    g_MpControlStyle[1] = state->mp_contstyle[1];
    g_MpControlStyle[2] = state->mp_contstyle[2];
    g_MpControlStyle[3] = state->mp_contstyle[3];
    g_MpAimSetting = state->aim_option;
    frontSetMpTeam(0, state->mp_flags[0]);
    frontSetMpTeam(1, state->mp_flags[1]);
    frontSetMpTeam(2, state->mp_flags[2]);
    frontSetMpTeam(3, state->mp_flags[3]);
}

void test_if_recording_demos_this_stage_load(enum LEVELID arg0, enum DIFFICULTY arg1)
{
    if (g_ramromRecordFlag != 0)
    {
        g_DemoFile = (ramromfilestructure *) ALIGN16_a((s32)ramrom_data_target);
        g_DemoFile->stagenum = arg0;
        g_DemoFile->difficulty = arg1;
        g_DemoFile->size_cmds = joyGetControllerCount();
        g_DemoFile->slotnum = record_slot_num;
        fileCopyCurrentSave(&g_DemoFile->savefile);
        copy_current_ingame_registers_before_ramrom_playback(g_DemoFile);
        recording_ramrom_flag = 1;
        ramrom_demo_related_6 = 1;
        joySetRecordFunc(record_player_input_as_packet);
        address_demo_loaded = INDY_RAMROM_DEMO_POINTER;
        romWrite(g_DemoFile, address_demo_loaded, 0xF0U);
        address_demo_loaded += sizeof(struct ramromfilestructure);
        g_ramromRecordFlag = 0;
        
        return;
    }
    
    if (g_ramromPlayBackFlag != 0)
    {
        fileSetDifficulty(g_DemoFile->difficulty);
        fileSetSoloStage(g_DemoFile->stagenum);
        fileUseDemoSave(&g_DemoFile->savefile);
        copy_current_ingame_registers_before_ramrom_playback((ramromfilestructure *) (ramrom_data_target + 0x110));
        copy_recorded_ramrom_registers_to_proper_place_ingame(g_DemoFile);
        is_ramrom_flag = 1;
        ramrom_demo_related_3 = 1;
        joySetPlaybackFunc(ramrom_replay_handler, g_DemoFile->size_cmds);
        joySetContDataIndex(1);
        g_ramromPlayBackFlag = 0;
    }
}





void stop_recording_ramrom(void)
{
    if (ramrom_demo_related_6 != 0)
    {
        finalize_ramrom_on_hw();
        joySetRecordFunc(0);
        ramrom_demo_related_6 = 0;
        recording_ramrom_flag = 0;
    }
}

void replay_recorded_ramrom_at_address(ramromfilestructure *demofile)
{
    address_demo_loaded = demofile;
    g_DemoFile = romCopyUnaligned(&ramrom_data_target, address_demo_loaded, sizeof(struct ramromfilestructure));
    address_demo_loaded += sizeof(ramromfilestructure);
    g_ramromPlayBackFlag = 1;
    fileSetSoloStage(g_DemoFile->stagenum);
    fileSetDifficulty(g_DemoFile->difficulty);
    frontSetMenu(MENU_RUN_STAGE,1);
}

void ramromFadeToTitle(void)
{
    if (playerGetCameraMode() != CAMERAMODE_FADE_TO_TITLE)
    {
        playerSetCameraMode(CAMERAMODE_FADE_TO_TITLE);
    }
}

void stop_demo_playback(void)
{
    if (ramrom_demo_related_6 != 0)
    {
        stop_recording_ramrom();
        return;
    }
    if (ramrom_demo_related_3 != 0)
    {
        copy_recorded_ramrom_registers_to_proper_place_ingame(ramrom_data_target + 0x110);
        joySetPlaybackFunc(0, -1);
        joySetContDataIndex(0);
        ramrom_demo_related_3 = 0;
        is_ramrom_flag = 0;
    }
}



void select_ramrom_to_play(void)
{
    s32 i;
    s32 temp_v0;

    temp_v0 = fileGetHighestStageUnlockedAnyFolder();

    for (i = 0; ramrom_table[i].fdata != NULL && temp_v0 >= ramrom_table[i].locked; i++)
    {}

    if (g_TestForceDemo >= 0 && g_TestForceDemo < i)
    {
        replay_recorded_ramrom_at_address(ramrom_table[g_TestForceDemo].fdata);
        return;
    }

    replay_recorded_ramrom_at_address(ramrom_table[randomGetNext() % i].fdata);
}





u32 check_ramrom_flags(void)
{
    if ((get_is_ramrom_flag() != 0) || (get_recording_ramrom_flag() != 0))
    {
        return g_DemoFile->slotnum;

    }
    return 0;
}



