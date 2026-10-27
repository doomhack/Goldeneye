#include <ultra64.h>
#include <music.h>
#include "game/audio/music_0D2720.h"
#include "game/menu/watchmenu.h"
#include "game/audio/mp_music.h"
#include "game/level/lv.h"

#ifdef VERSION_EU
#define MP_MUSIC_FRAMERATE 50
#else
#define MP_MUSIC_FRAMERATE 60
#endif

// bss
//CODE.bss:8008C600
s32 g_LevelMusicStage;
//CODE.bss:8008C604
s32 g_LevelMusicPausedState;

//CODE.bss:8008C608
s32 g_LevelMusicSlotActive[4];

//CODE.bss:8008C618
s32 g_LevelMusicSlotLinger[4];

//CODE.bss:8008C628
s32 g_LevelMusicSlotTotal[4];


// data
//D:800484C0
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



void levelMusicSetState(MUSICSTATE arg0)
{
    s32 old_mission_state = g_LevelMusicState;

    g_LevelMusicState = arg0;

    switch (old_mission_state)
    {
        case MUSICSTATE_SILENT:
        switch (arg0)
        {
#if defined(VERSION_US) && !defined(BACKPORT_R1)
            case MUSICSTATE_SILENT:
                return;
            case MUSICSTATE_LEVEL:
                musicTrack1ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack1Fade = 0;
                musicTrack1Play(levelMusicGetTrack(g_LevelMusicStage));
                return;
            case MUSICSTATE_XTRACK:
                break;
            case MUSICSTATE_WATCH:
                break;
            case MUSICSTATE_LEVEL_AMBIENT: // switch 2
                musicTrack1ApplySeqpVol(levelMusicGetVolume());
                g_musicXTrack1Fade = 0;
                musicTrack1Play(levelMusicGetTrack(g_LevelMusicStage));
                musicTrack3ApplySeqpVol(levelMusicGetFxVolume());
                g_musicXTrack3Fade = 0;
                musicTrack3Play(levelMusicGetAmbientTrack(g_LevelMusicStage));
                return;
            case MUSICSTATE_XTRACK_AMBIENT:
                break;
            case MUSICSTATE_MP_DEATH:
                break;
#endif

#if defined(VERSION_EU) || defined(BACKPORT_R1)
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
#endif
        }

        case MUSICSTATE_LEVEL:
        switch (arg0)
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
        }

        case MUSICSTATE_XTRACK:
        switch (arg0)
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
        }

        case MUSICSTATE_WATCH:
        switch (arg0)
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
        }

        case MUSICSTATE_LEVEL_AMBIENT:
        switch (arg0)
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
        }

        case MUSICSTATE_XTRACK_AMBIENT:
        switch (arg0)
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
        }

        case MUSICSTATE_MP_DEATH:
        switch (arg0)
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
        }

        default:
        break;
    }
#ifdef DEBUG
    osSyncPrintf("Invalid current music state %d\n", arg0);
#endif

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
    s32 var_t2;

    var_t2 = 0;

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
                        var_t2 = 1;
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
            if (var_t2 == 0)
            {
                levelMusicEndXTrack();
            }
        }
        else if (var_t2 != 0)
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

