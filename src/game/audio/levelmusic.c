/**
 * Level music: each level's main, X and ambient tracks (g_LevelMusic), the music state
 * machine that starts and stops them (levelMusicSetState), and the music slots the AI
 * scripts use to bring in the X track (levelMusicSlotStart, levelMusicTick).
 * See docs/subsystems/audio.md.
 */
#include <ultra64.h>
#include <music.h>
#include "game/audio/levelmusic.h"
#include "game/menu/watchmenu.h"
#include "game/level/lv.h"
#include "bondconstants.h"
#include "random.h"
#include <snd.h>
#include <PR/libaudio.h>
#include "game/player/bondview.h"

#ifdef VERSION_EU
#define MP_MUSIC_FRAMERATE 50
#else
#define MP_MUSIC_FRAMERATE 60
#endif

// bss
s32 g_LevelMusicStage;
s32 g_LevelMusicPausedState;

s32 g_LevelMusicSlotActive[4];

s32 g_LevelMusicSlotLinger[4];

s32 g_LevelMusicSlotTotal[4];


// data
s32 g_LevelMusicState = MUSICSTATE_SILENT;



u16 levelMusicGetVolume(void)
{
    return optionsGetMusicVolume();
}

u16 levelMusicGetFxVolume(void)
{
    return optionsGetFxVolume();
}


MUSICSTATE levelMusicGetState(void)
{
    return g_LevelMusicState;
}



void levelMusicSetState(MUSICSTATE state)
{
    s32 old_mission_state = g_LevelMusicState;

    g_LevelMusicState = state;

    switch (old_mission_state)
    {
        case MUSICSTATE_SILENT:
        switch (state)
        {

            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL: // switch 2
                musicTrack1ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack1Fade = 0;
                musicTrack1Play(levelMusicGetTrack(g_LevelMusicStage));
                return;
            case MUSICSTATE_XTRACK:
                break;
            case MUSICSTATE_WATCH:
                break;
            case MUSICSTATE_LEVEL_AMBIENT:
                musicTrack1ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack1Fade = 0;
                musicTrack1Play(levelMusicGetTrack(g_LevelMusicStage));
                musicTrack3ApplySeqpVol(levelMusicGetFxVolume());
                g_musicXTrack3Fade = 0;
                musicTrack3Play(levelMusicGetAmbientTrack(g_LevelMusicStage));
                return;
                break;
            case MUSICSTATE_XTRACK_AMBIENT:
                break;
            case MUSICSTATE_MP_DEATH:
                break;

            default:
                break;
        }

        case MUSICSTATE_LEVEL:
        switch (state)
        {
            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL:
                break;
            case MUSICSTATE_XTRACK:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(levelMusicGetXTrack(g_LevelMusicStage));
                musicTrack1FadeOut(0.5f);
                return;
            case MUSICSTATE_WATCH:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(0x18);
                musicTrack1FadeOut(0.5f);
                return;
            case MUSICSTATE_XTRACK_AMBIENT:
                break;
            case MUSICSTATE_MP_DEATH:
                musicTrack1FadeOut(0.02f);
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(0x3A);
                return;

            default:
                break;
        }

        case MUSICSTATE_XTRACK:
        switch (state)
        {
            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL:
                musicTrack1FadeIn(0.5f, levelMusicGetVolume());
                musicTrack2FadeOut(0.5f);
                return;
            case MUSICSTATE_XTRACK:
                return;
            case MUSICSTATE_WATCH:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(0x18);
                return;
            case MUSICSTATE_LEVEL_AMBIENT:
                break;
            case MUSICSTATE_XTRACK_AMBIENT:
                break;
            case MUSICSTATE_MP_DEATH:
                break;

            default:
                break;
        }

        case MUSICSTATE_WATCH:
        switch (state)
        {
            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL:
                musicTrack1FadeIn(1.0f, levelMusicGetVolume());
                musicTrack2FadeOut(1.0f);
                return;
            case MUSICSTATE_XTRACK:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(levelMusicGetXTrack(g_LevelMusicStage));
                return;
            case MUSICSTATE_WATCH:
                break;
            case MUSICSTATE_LEVEL_AMBIENT:
                musicTrack1FadeIn(1.0f, levelMusicGetVolume());
                musicTrack3FadeIn(1.0f, levelMusicGetFxVolume());
                musicTrack2FadeOut(1.0f);
                return;
            case MUSICSTATE_XTRACK_AMBIENT:
                musicTrack3FadeIn(1.0f, levelMusicGetFxVolume());
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(levelMusicGetXTrack(g_LevelMusicStage));
                return;
            case MUSICSTATE_MP_DEATH:
                break;

            default:
                break;
        }

        case MUSICSTATE_LEVEL_AMBIENT:
        switch (state)
        {
            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL:
                break;
            case MUSICSTATE_XTRACK:
                break;
            case MUSICSTATE_WATCH:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(0x18);
                musicTrack1FadeOut(0.5f);
                musicTrack3FadeOut(0.5f);
                return;
            case MUSICSTATE_LEVEL_AMBIENT:
                break;
            case MUSICSTATE_XTRACK_AMBIENT:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(levelMusicGetXTrack(g_LevelMusicStage));
                musicTrack1FadeOut(0.5f);
                return;
            case MUSICSTATE_MP_DEATH:
                break;

            default:
                break;
        }

        case MUSICSTATE_XTRACK_AMBIENT:
        switch (state)
        {
            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL:
                break;
            case MUSICSTATE_XTRACK:
                break;
            case MUSICSTATE_WATCH:
                musicTrack2ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack2Fade = 0;
                musicTrack2Play(0x18);
                musicTrack3FadeOut(0.5f);
                return;
            case MUSICSTATE_LEVEL_AMBIENT:
                musicTrack1FadeIn(0.5f, levelMusicGetVolume());
                musicTrack2FadeOut(0.5f);
                return;
            case MUSICSTATE_XTRACK_AMBIENT:
                break;
            case MUSICSTATE_MP_DEATH:
                break;

            default:
                break;
        }

        case MUSICSTATE_MP_DEATH:
        switch (state)
        {
            case MUSICSTATE_SILENT:
                musicTrack1Stop();
                musicTrack2Stop();
                musicTrack3Stop();
                return;
            case MUSICSTATE_LEVEL:
                musicTrack1FadeIn(2.0f, levelMusicGetVolume());
                musicTrack2FadeOut(2.0f);
                return;
            case MUSICSTATE_XTRACK:
                break;
            case MUSICSTATE_WATCH:
                break;
            case MUSICSTATE_LEVEL_AMBIENT:
                break;
            case MUSICSTATE_XTRACK_AMBIENT:
                break;
            case MUSICSTATE_MP_DEATH:
                g_musicXTrack2Fade = 0;
                musicTrack2Play(0x3A);
                return;

            default:
                break;
        }

        default:
        break;
    }

    while(1)
    {
        // error
    }
}




void levelMusicStart(s32 stagenum)
{
    musicTrack1Stop();
    musicTrack2Stop();
    musicTrack3Stop();
    g_LevelMusicState = MUSICSTATE_SILENT;
    g_LevelMusicStage = stagenum;

    if (levelMusicGetAmbientTrack(g_LevelMusicStage) < 0)
    {
        levelMusicSetState(MUSICSTATE_LEVEL);
    }
    else
    {
        levelMusicSetState(MUSICSTATE_LEVEL_AMBIENT);
    }

    return;
}

void levelMusicStop(void)
{
    levelMusicSetState(MUSICSTATE_SILENT);
}

void levelMusicStartXTrack(void)
{
    if (levelMusicGetAmbientTrack(g_LevelMusicStage) < 0)
    {
        levelMusicSetState(MUSICSTATE_XTRACK);
    }
    else
    {
        levelMusicSetState(MUSICSTATE_XTRACK_AMBIENT);
    }
}

void levelMusicEndXTrack(void)
{
    if (levelMusicGetAmbientTrack(g_LevelMusicStage) < 0)
    {
        levelMusicSetState(MUSICSTATE_LEVEL);
    }
    else
    {
        levelMusicSetState(MUSICSTATE_LEVEL_AMBIENT);
    }
}

void levelMusicPause(void)
{
    g_LevelMusicPausedState = g_LevelMusicState;
    levelMusicSetState(MUSICSTATE_WATCH);
}

void levelMusicResume(void)
{
    levelMusicSetState(g_LevelMusicPausedState);
}

void levelMusicResetSlots(void)
{
    s32 i;

    for (i=0; i<4; i++)
    {
        g_LevelMusicSlotActive[i] = 0;
        g_LevelMusicSlotLinger[i] = 0;
        g_LevelMusicSlotTotal[i] = 0;
    }
}






void levelMusicTick(void)
{
    s32 i;
    s32 wantXTrack;

    wantXTrack = 0;

    for (i=0; i<4; i++)
    {
        if (g_LevelMusicSlotActive[i] || g_LevelMusicSlotLinger[i] > 0)
        {
            if (g_LevelMusicSlotLinger[i] >= g_ClockTimer)
            {
                g_LevelMusicSlotLinger[i] -= g_ClockTimer;
            }
            else
            {
                g_LevelMusicSlotLinger[i] = 0;
            }

            if (g_LevelMusicSlotTotal[i])
            {
                if (g_LevelMusicSlotTotal[i] >= g_ClockTimer)
                {
                    g_LevelMusicSlotTotal[i] -= g_ClockTimer;
                }
                else
                {
                    g_LevelMusicSlotTotal[i] = 0;
                }

                if (g_LevelMusicSlotTotal[i])
                {
                    if ((g_LevelMusicSlotActive[i]) || (g_LevelMusicSlotLinger[i]))
                    {
                        wantXTrack = 1;
                    }
                }
                else
                {
                    g_LevelMusicSlotActive[i] = 0;
                }
            }
        }
    }

    if (g_ClockTimer != 0)
    {
        if ((levelMusicGetState() == MUSICSTATE_XTRACK) || (levelMusicGetState() == MUSICSTATE_XTRACK_AMBIENT))
        {
            if (wantXTrack == 0)
            {
                levelMusicEndXTrack();
            }
        }
        else if (wantXTrack != 0)
        {
            levelMusicStartXTrack();
        }
    }

}





void levelMusicSlotStart(s32 slot, s32 min, s32 sec)
{
    if ((g_LevelMusicSlotActive)[slot] == FALSE)
    {
        (g_LevelMusicSlotActive)[slot] = TRUE;
        (g_LevelMusicSlotLinger)[slot] = min * MP_MUSIC_FRAMERATE;
        (g_LevelMusicSlotTotal)[slot] = sec * MP_MUSIC_FRAMERATE;
    }
}

void levelMusicSlotStop(s32 slot)
{
    s32 i;

    if (-1 < slot)
    {
        (g_LevelMusicSlotActive)[slot] = 0;
        return;
    }

    for (i=0; i<4; i++)
    {
        g_LevelMusicSlotActive[i] = 0;
        g_LevelMusicSlotLinger[i] = 0;
        g_LevelMusicSlotTotal[i] = 0;
    }
}


struct music_setup
{
  s16 stage_id;
  s16 main_music;
  s16 bg_sound;
  s16 xtrack;
};

// data
struct music_setup g_LevelMusic[] = {
    { LEVELID_BUNKER1,   M_BUNKER1,      0xFFFF,   M_BUNKER1X },
    { LEVELID_SILO,      M_SILO,         0xFFFF,   M_SILOX },
    { LEVELID_STATUE,    M_STATUE,       0xFFFF,   M_STATUEPART },
    { LEVELID_CONTROL,   M_CONTROL,      0xFFFF,   M_ELEVATOR_CONTROL },
    { LEVELID_ARCHIVES,  M_ARCHIVES,     0xFFFF,   M_ARCHIVESX },
    { LEVELID_TRAIN,     M_TRAIN,        0xFFFF,   M_TRAINX },
    { LEVELID_FRIGATE,   M_FRIGATE,      0xFFFF,   M_END_SOMETHING },
    { LEVELID_BUNKER2,   M_BUNKER2,      0xFFFF,   M_BUNKER2X },
    { LEVELID_AZTEC,     M_AZTEC,        0xFFFF,   M_AZTECX },
    { LEVELID_STREETS,   M_STREETS,      0xFFFF,   M_STREETSX },
    { LEVELID_DEPOT,     M_DEPOT,        0xFFFF,   M_DEPOTX },
    { LEVELID_EGYPT,     M_EGYPTIAN,     0xFFFF,   M_EGYPTX },
    { LEVELID_DAM,       M_DAM,          0xFFFF,   M_WIND },
    { LEVELID_FACILITY,  M_FACILITY,     0xFFFF,   M_FACILITYX },
    { LEVELID_RUNWAY,    M_RUNWAY,       0xFFFF,   M_RUNWAYPLANE },
    { LEVELID_SURFACE,   M_SURFACE1,     0xFFFF,   M_WIND },
    { LEVELID_JUNGLE,    M_JUNGLE,       0xFFFF,   M_JUNGLEX },
    { LEVELID_CAVERNS,   M_WATERCAVERNS, 0xFFFF,   M_ELEVATOR_WC },
    { LEVELID_CITADEL,   M_CITADEL,      0xFFFF,   0xFFFF },
    { LEVELID_CRADLE,    M_CRADLE,       0xFFFF,   M_CRADLEX },
    { LEVELID_SHO,       M_SURFACE2,     0xFFFF,   0xFFFF },
    { LEVELID_SURFACE2,  M_SURFACE2,     M_WIND,   M_SURFACE2END },
    { LEVELID_CUBA,      M_CUBA,         0xFFFF,   0xFFFF },
    { 0 }
};

s16 g_LevelMusicRandom[] = {
    M_TRAIN,
    M_DEPOT,
    M_MPTHEME,
    M_CITADEL,
    M_FACILITY,
    M_CONTROL,
    M_DAM,
    M_FRIGATE,
    M_ARCHIVES,
    M_SILO,
    M_MPTHEME2,
    M_STREETS,
    M_BUNKER1,
    M_BUNKER2,
    M_STATUE,
    M_ELEVATOR_CONTROL,
    M_CRADLE,
    M_ELEVATOR_WC,
    M_EGYPTIAN,
    M_AZTEC,
    M_WATERCAVERNS,
    M_SURFACE2,
    M_TRAINX,
    M_FACILITYX,
    M_DEPOTX,
    M_CONTROLX,
    M_WATERCAVERNSX,
    M_DAMX,
    M_FRIGATEX,
    M_ARCHIVESX,
    M_SILOX,
    M_STREETSX,
    M_BUNKER1X,
    M_BUNKER2X,
    M_JUNGLEX,
    M_STATUEX,
    M_AZTECX,
    M_EGYPTX,
    M_CRADLEX,
    M_RUNWAY,
    M_MPTHEME3,
    M_RUNWAYX,
    M_SURFACE1,
    M_NONE
};


/**
*/
s32 levelMusicGetTrack(s32 stage)
{
    s32 index;
    
    for (index = 0; g_LevelMusic[index].stage_id != 0; index++)
    {
        if (g_LevelMusic[index].stage_id == stage)
        {
            s32 count;
            s32 result;
            
            if (g_LevelMusic[index].main_music == -1)
            {
                count = 0;
                while (g_LevelMusicRandom[count] != M_NONE)
                {
                    count++;
                }

                result = g_LevelMusicRandom[randomGetNext() % count];
                return result;
            }

            result = g_LevelMusic[index].main_music;
            return result;
        }
    }

    if(1)
    {
        s32 count;
        s32 result;
        
        count = 0;
        
        while (g_LevelMusicRandom[count] != M_NONE)
        {
            count++;
        }
    
        result = g_LevelMusicRandom[randomGetNext() % count];
        return result;
    }
}





s32 levelMusicGetAmbientTrack(s32 stageID)
{
    s32 i;
    for (i=0; g_LevelMusic[i].stage_id!=0; i++)
    {
        if (stageID == g_LevelMusic[i].stage_id) 
        {
            return g_LevelMusic[i].bg_sound;
        }
    }

    return -1;
}


s32 levelMusicGetXTrack(s32 stageID)
{
    s32 i;
    for (i=0; g_LevelMusic[i].stage_id!=0; i++)
    {
        if (stageID == g_LevelMusic[i].stage_id) 
        {
            return g_LevelMusic[i].xtrack;
        }
    }

    return -1;
}


void levelStopTankSfx(void) {
    if (g_TankSfxState[0] != 0)
    {
        if (sndGetPlayingState(g_TankSfxState[0]) != AL_STOPPED)
        {
            sndDeactivate(g_TankSfxState[0]);
        }
    }

    if (g_TankSfxState[1] != 0)
    {
        if (sndGetPlayingState(g_TankSfxState[1]) != AL_STOPPED)
        {
            sndDeactivate(g_TankSfxState[1]);
        }
    }

}
