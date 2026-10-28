
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
#include "game/menu/mp_weapon.h"
#include "game/player/bondview.h"

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
s32 ramrom_replay_handler(struct contsample *samples, s32 index);
void record_player_input_as_packet(struct contsample *samples, s32 lastread, s32 lastwrite);
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


void record_player_input_as_packet(struct contsample *samples, s32 lastread, s32 lastwrite)
{
    s32 bufaddr;
    s32 numcontrollers;
    s32 cont;
    s32 sample;
    u8 checksum;
    s32 count;
    struct ramrom_blockbuf *entry;
    s32 size;

    bufaddr = ALIGN16_a((s32)&ramrom_data_target[0x1f8]);
    numcontrollers = g_DemoFile->size_cmds;

    count = 0;
    checksum = 0;
    
    ramrom_blkbuf_2 = (struct ramrom_seed *)bufaddr;
    ramrom_blkbuf_3 = (struct ramrom_blockbuf *)(ramrom_blkbuf_2 + 1);

    // loop structure based on: void joyConsumeSamples(struct contdata *contdata)
    if (lastread != lastwrite)
    {
        sample = (s32) (lastread + 1) % CONTSAMPLE_LEN;
        while (1)
        {
            for (cont = 0; cont < numcontrollers; cont++)
            {
                entry = ramrom_blkbuf_3 + (count * numcontrollers) + cont;

                entry->stick_x = samples->pads[sample*4 + cont].stick_x;
                entry->stick_y = samples->pads[sample*4 + cont].stick_y;
                entry->button_low = samples->pads[sample*4 + cont].button & 0xFF;
                entry->button_high = samples->pads[sample*4 + cont].button >> 8;
    
                checksum += (u8) (
                    (u8)entry->stick_x
                    + (u8)entry->stick_y
                    + entry->button_low
                    + entry->button_high);
            }
            count++;

            if (sample == lastwrite)
            {
                break;
            }

            sample = (s32) (sample + 1) % CONTSAMPLE_LEN;
        }
    }
        
    ramrom_blkbuf_2->count = count;
    ramrom_blkbuf_2->speedframes = g_FrameDelta;
    ramrom_blkbuf_2->randseed = g_randomSeed;
    
    checksum += (u8) ((u8)ramrom_blkbuf_2->speedframes + (u8)ramrom_blkbuf_2->count + ramrom_blkbuf_2->randseed);
    ramrom_blkbuf_2->check = checksum;

    size = (numcontrollers * 4 * count) + sizeof(s32);

    romWrite((void *) ramrom_blkbuf_2, address_demo_loaded, ALIGN16_a(size));

    address_demo_loaded += align_addr_even(size + 1);
}




s32 ramrom_replay_handler(struct contsample *samples, s32 index)
{
    s32 frame;
    s32 cont;
    struct ramrom_blockbuf *entry;
    u8 checksum;
    s32 numcontrollers;
    s32 count;

    checksum = 0;
    numcontrollers = (s32) g_DemoFile->size_cmds;
    count = ramrom_blkbuf_2->count;

    for (frame = 0; frame < count; frame++)
    {
        index = (s32) (index + 1) % CONTSAMPLE_LEN;

        for (cont = 0; cont < MAXCONTROLLERS; cont++)
        {
            if (cont < numcontrollers)
            {
                entry = ramrom_blkbuf_3 + (frame * numcontrollers) + cont;

                samples->pads[index * 4 + cont].stick_x = entry->stick_x;
                samples->pads[index * 4 + cont].stick_y = entry->stick_y;
                samples->pads[index * 4 + cont].button = (entry->button_high << 8) | entry->button_low;

                checksum += (u8)((u8)entry->stick_x + (u8)entry->stick_y + entry->button_low + entry->button_high);
            }
            else
            {
                samples->pads[index * 4 + cont].stick_x = 0;
                samples->pads[index * 4 + cont].stick_y = 0;
                samples->pads[index * 4 + cont].button = 0;
            }
        }
    }

    if (ramrom_blkbuf_2->randseed != (u8)g_randomSeed)
    {
        // The game has drifted from the recording.
        g_TestDemoDesynced = TRUE;
        ramromFadeToTitle();
    }

    checksum += (u8)((u8)ramrom_blkbuf_2->speedframes + (u8)ramrom_blkbuf_2->count + ramrom_blkbuf_2->randseed);
    if (ramrom_blkbuf_2->check != checksum)
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

    return index;
}



void iterate_ramrom_entries_handle_camera_out(void)
{
    s32 endtime;
    s32 count;
    
    ramrom_blkbuf_2 = romCopyUnaligned(ramrom_data_target + 0x1F8, address_demo_loaded, sizeof(struct ramrom_seed));

    count = ramrom_blkbuf_2->count;
    if (count > 0)
    {
        ramrom_blkbuf_3 = romCopyUnaligned(
            ramrom_data_target + 0x21E,
            address_demo_loaded + 4,
            g_DemoFile->size_cmds * sizeof(struct ramrom_blockbuf) * ramrom_blkbuf_2->count);
    }

    count = ramrom_blkbuf_2->count;
    if (count == 0 && ramrom_blkbuf_2->speedframes == 0)
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
    endtime = g_DemoFile->totaltime_ms - 0x3C;
    if ((g_GlobalTimer >= endtime) && ((g_GlobalTimer - g_ClockTimer) < endtime))
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

void test_if_recording_demos_this_stage_load(enum LEVELID stage, enum DIFFICULTY difficulty)
{
    if (g_ramromRecordFlag != 0)
    {
        g_DemoFile = (ramromfilestructure *) ALIGN16_a((s32)ramrom_data_target);
        g_DemoFile->stagenum = stage;
        g_DemoFile->difficulty = difficulty;
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
    s32 highestunlocked;

    highestunlocked = fileGetHighestStageUnlockedAnyFolder();

    for (i = 0; ramrom_table[i].fdata != NULL && highestunlocked >= ramrom_table[i].locked; i++)
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



