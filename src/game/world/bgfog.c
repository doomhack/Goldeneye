/**
 * The level's environment: fog distances and colour, the sky colour and the cloud and
 * water images, chosen per level and player count from fog_tables. The record after the
 * level's own is an alternative that AI scripts and toxic gas blend towards
 * (fogSwitchToSolosky2).
 */

#include <ultra64.h>

#include <bondtypes.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/world/bg.h"
#include "game/world/bgfog.h"
#include "game/player/player.h"
#include "game/menu/watchmenu.h"
#include <limits.h>

s32 g_FogSkyIsEnabled;

NearFogValues *g_NearFogValuesP;

/**
 * Address 0x800825C8. Private struct to this file only
*/
struct FogDetails
{
    f32 g_CurFogDetails;
    f32 scaled_far_fog_dist;
    u32 unk08;
    u32 unk0c;
    f32 far_fog_dist_scaled;
    f32 near_fog_dist_scaled;
} g_CurFogDetails;

f32 g_FarFogIntensity;

f32 g_DifferenceFromFarFogIntensity;

EnvironmentRecord *g_EnvironmentFoundp;

EnvironmentRecord * g_EnvironmentMainp;

EnvironmentRecord * g_EnvironmentAltp;

#if defined(VERSION_EU)
NearFogValues eu_loadCurrentNearFog;
#endif


f32 g_ScaledFarFogIntensity = FLT_MAX;

f32 g_ScaledDifferenceFromFarFogIntensity = 0.0;

CurrentEnvironmentRecord g_CurrentEnvironment = {
    0x384, // s32 DifferenceFromFarIntensity;
    0x3e8, // u32 FarIntensity;
        0,     // u8  Red;
        0,     // u8  Green;
        0,     // u8  Blue;
        0,     // u8  Clouds;
        0.0f,  // f32 CloudRepeat;
        0,     // s16 SkyImageId;
        0,
        0.0f,  // f32 CloudRed;
        0.0f,  // f32 CloudGreen;
        0.0f,  // f32 CloudBlue;
        0,     // u8  IsWater;
        {0,0,0},
        0.0f,  // f32 WaterRepeat;
        0,     // s16 WaterImageId;
        0,
        0.0f,  // f32 WaterRed;
        0.0f,  // f32 WaterGreen;
        0.0f,  // f32 WaterBlue;
        0.0f  // f32 WaterConcavity;
};

#if defined(VERSION_EU)
EnvironmentRecord fog_tables[] = {
     //stageID                              blendmultiplier    farfog    nearfog  mvisrng  mobfnrng  dif_ght  far_alight    red     green     blue    clouds   cloudrept  skymid, cloudred   green    blue   iswater  waterrepeat  waterid  water red,green,blue  waterconcavity
    {LEVELID_STATUE                             ,        {15,      3500,    {2000,     2500,    2000}},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      5000,       0,      170,    100,     40,        0,       -1000,       0,       0,     0,    0,    30.0} },
    {LEVELID_CONTROL                            ,        {10,     10000,    {2500,     5000,     800}},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,       0,        0,      0,      0,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_ARCHIVES                           ,        {10,      3000,    {2000,     3000,     500}},    {0x3E4,    0x3E8},       {0,       0,       0,        1,      5000,       0,      255,    255,    255,        0,       -1000,       0,       0,     0,    0,     0.0} },
    {LEVELID_TRAIN                              ,        {10,      1500,       {0,        0,       0}},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      5000,       0,      255,    255,    255,        0,           0,       1,       0,   255,  255,    25.0} },
    {LEVELID_TRAIN + ENVIRONMENTDATA_ALT        ,        {20,     15000,       {0,        0,       0}},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      5000,       0,      255,    255,    255,        0,           0,       1,       0,   255,  255,    25.0} },
    {LEVELID_STREETS                            ,        {10,      7500,    {5000,     6000,    1000}},    {0x3E4,    0x3E8},    {0x10,    0x18,    0x20,        1,      5000,       0,      225,    175,    100,        0,       -1000,       0,       0,     0,    0,    25.0} },
    {LEVELID_DEPOT                              ,        {10,      3000,    {1600,     2000,     800}},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      7500,       0,       70,    199,    186,        0,       -1000,       1,       0,   255,  255,    25.0} },
    {LEVELID_COMPLEX                            ,        {10,      5000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,       0,      220,      0,     20,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_DAM                                ,         {5,     15000,    {3333,     4444,     600}},    {0x3E3,    0x3E8},    {0x10,    0x30,    0x60,        1,      5000,       0,      255,    255,    255,        0,       -1000,       0,       0,     0,    0,     0.0} },
    {LEVELID_DAM + ENVIRONMENTDATA_CINEMA       ,        {30,     15000,    {3333,     4444,     600}},    {0x3E3,    0x3E8},    {0x10,    0x30,    0x60,        1,      5000,       0,      255,    255,    255,        0,       -1000,       0,       0,     0,    0,     0.0} },
    {LEVELID_FACILITY                           ,        {10,      5000,       {0,        0,       0}},    {0x3DE,    0x3E8},    {0x10,    0x20,    0x10,        0,      5000,       0,      255,    255,    255,        0,       -1000,       0,       0,     0,    0,     0.0} },
    {LEVELID_FACILITY + ENVIRONMENTDATA_ALT     ,        {10,      1000,       {0,        0,       0}},    {0x3DE,    0x3E8},    {0x40,    0x80,    0x40,        0,      5000,       0,      255,    255,    255,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_RUNWAY                             ,        {10,     15000,    {6000,     8000,     800}},    {0x3E4,    0x3E8},    {0x10,    0x30,    0x40,        1,      5000,       0,       25,     25,     25,        0,       -1000,       0,       0,     0,    0,     0.0} },
    {LEVELID_SURFACE                            ,         {2,      2500,    {4444,     5555,     800}},    {0x3E4,    0x3E8},    {0x60,    0x60,    0x80,        1,     10000,       0,      240,    120,    30,         0,           0,       1,       0,   255,  255,     7.0} },
    {LEVELID_JUNGLE                             ,        {10,      2500,    {1500,     2500,    1000}},    {0x3E4,    0x3E8},    {0x18,    0x20,       0,        0,      5000,       0,      255,    255,    255,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_TEMPLE                             ,        {10,      6000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x28,        1,     10000,       0,      160,    160,    190,        0,        -500,       0,       0,     0,    0,     0.0} },
    {LEVELID_CAVERNS                            ,        {10,      6000,       {0,        0,       0}},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,       0,      255,    255,    255,        0,       -3000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CRADLE                             ,        {10,      9500,    {3333,     4444,     800}},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,       0,      255,    255,      0,        0,      -10000,       1,       0,   255,  255,     0.0} },
    {LEVELID_SURFACE2                           ,         {2,      2000,    {2500,     3055,     750}},    {0x3BD,    0x3E8},    {0x20,    0x10,    0x10,        1,      5000,       0,       58,     17,      0,        0,       -1000,       1,       0,   255,  255,    20.0} },
    {LEVELID_SURFACE2 + ENVIRONMENTDATA_CINEMA  ,         {2,      8000,    {6000,     8000,     800}},    {0x3DF,    0x3E8},    {0x20,    0x10,    0x10,        1,      5000,       0,       58,     17,      0,        0,       -1000,       1,       0,   255,  255,     0.0} },
    {LEVELID_BUNKER2                            ,        {10,     10000,    {1000,    15000,     750}},    {0x3E4,    0x41A},    {0x10,       0,       0,        1,      5000,       0,       58,     17,      0,        0,       -1000,       1,       0,   255,  255,     0.0} },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_2,        {10,      2500,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x80,    0x78,    0x70,        0,         0,       0,      130,    120,    110,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_3,        {10,      2000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x80,    0x78,    0x70,        0,         0,       0,      130,    120,    110,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_4,        {10,      1500,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x80,    0x78,    0x70,        0,         0,       0,      130,    120,    110,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_2 ,        {10,      7000,    {3333,     4444,     800}},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,       0,      255,    255,      0,        0,      -10000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_3 ,        {10,      5500,    {3333,     4444,     800}},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,       0,      255,    255,      0,        0,      -10000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_4 ,        {10,      4000,    {3333,     4444,     800}},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,       0,      255,    255,      0,        0,      -10000,       1,       0,   255,  255,     0.0} },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_2 ,        {10,      6000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x18,        1,     10000,       0,      120,    120,    120,        0,        -500,       0,       0,     0,    0,     0.0} },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_3 ,        {10,      6000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x18,        1,     10000,       0,      120,    120,    120,        0,        -500,       0,       0,     0,    0,     0.0} },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_4 ,        {10,      6000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x18,        1,     10000,       0,      120,    120,    120,        0,        -500,       0,       0,     0,    0,     0.0} },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_2,        {10,      7500,       {0,        0,       0}},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,       0,      255,    255,    255,        0,       -3000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_3,        {10,      6000,       {0,        0,       0}},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,       0,      255,    255,    255,        0,       -3000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_4,        {10,      5000,       {0,        0,       0}},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,       0,      255,    255,    255,        0,       -3000,       1,       0,   255,  255,     0.0} },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_2,        {10,      7500,       {0,        0,       0}},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,       0,        0,      0,      0,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_3,        {10,      6000,       {0,        0,       0}},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,       0,        0,      0,      0,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_4,        {10,      5000,       {0,        0,       0}},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,       0,        0,      0,      0,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_CITADEL                            ,        {10,     20000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,       0,      255,    108,      0,        0,       -1000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_2,        {10,     20000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,       0,      255,    108,      0,        0,       -1000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_3,        {10,     20000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,       0,      255,    108,      0,        0,       -1000,       1,       0,   255,  255,     0.0} },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_4,        {10,     20000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,       0,      255,    108,      0,        0,       -1000,       1,       0,   255,  255,     0.0} },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_2,        {10,      5000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,       0,      220,      0,     20,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_3,        {10,      5000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,       0,      220,      0,     20,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_4,        {10,      5000,       {0,        0,       0}},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,       0,      220,      0,     20,        0,           0,       0,       0,     0,    0,     0.0} },
    {LEVELID_AZTEC                              ,        {10,     15000,    {3000,     4000,     600}},    {0x3E4,    0x3E8},       {0,       0,       0,        1,      7500,       0,       83,     72,     65,        0,       -5000,       0,       0,     0,    0,     0.0} },
    {LEVELID_AZTEC + ENVIRONMENTDATA_ALT        ,        {30,     15000,    {3000,     4000,     600}},    {0x3E4,    0x3E8},       {0,       0,       0,        1,      7500,       0,       83,     72,     65,        0,       -5000,       0,       0,     0,    0,     0.0} },
    {LEVELID_EGYPT                              ,        {10,     20000,    {3000,     4000,     600}},    {0x3E4,    0x3E8},    {0x10,    0x30,    0x60,        1,      5000,       0,      255,    255,    255,        0,       -5000,       0,       0,     0,    0,     0.0} },
    {LEVELID_EGYPT + ENVIRONMENTDATA_ALT        ,        {10,     10000,    {3000,     4000,     600}},    {0x3CC,    0x3E8},       {0,       0,       0,        1,      7500,       0,       83,     10,     10,        0,       -5000,       0,       0,     0,    0,     0.0} },
    {ENVIRONMENTDATA_END},
};
#else
EnvironmentRecord fog_tables[] = {
     //stageID                              blendmultiplier    farfog    nearfog  mvisrng  mobfnrng  mnvisrng   intensity  dif_ght  far_alight    red     green     blue    clouds   cloudrept skyimid reserved  cloudred   green    blue   iswater  padding[3]  waterrepeat  waterid reserved2   water red,green,blue  waterconcavity
    {LEVELID_STATUE                             ,        {15,      3500,    {2000,     2500,    2000},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      5000,    0,        0,        170,    100,     40,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,    30.0} },
    {LEVELID_CONTROL                            ,        {10,     10000,    {2500,     5000,     800},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_ARCHIVES                           ,        {10,      3000,    {2000,     3000,     500},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        1,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_TRAIN                              ,        {10,      1500,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},             0,       1,        0,        0,   255,  255,    25.0} },
    {LEVELID_TRAIN + ENVIRONMENTDATA_ALT        ,        {20,     15000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},             0,       1,        0,        0,   255,  255,    25.0} },
    {LEVELID_STREETS                            ,        {10,      7500,    {5000,     6000,    1000},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x10,    0x18,    0x20,        1,      5000,    0,        0,        225,    175,    100,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,    25.0} },
    {LEVELID_DEPOT                              ,        {10,      3000,    {1600,     2000,     800},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       8,        1,      7500,    0,        0,         70,    199,    186,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,    25.0} },
    {LEVELID_COMPLEX                            ,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_DAM                                ,         {5,     15000,    {3333,     4444,     600},        0,      0x3E7},    {0x3E3,    0x3E8},    {0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_DAM + ENVIRONMENTDATA_CINEMA       ,        {30,     15000,    {3333,     4444,     600},        0,      0x3E7},    {0x3E3,    0x3E8},    {0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_FACILITY                           ,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3DE,    0x3E8},    {0x10,    0x20,    0x10,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_FACILITY + ENVIRONMENTDATA_ALT     ,        {10,      1000,       {0,        0,       0},        0,      0x3E7},    {0x3DE,    0x3E8},    {0x40,    0x80,    0x40,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_RUNWAY                             ,        {10,     15000,    {6000,     8000,     800},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x10,    0x30,    0x40,        1,      5000,    0,        0,         25,     25,     25,        0,    {0,0,0},         -1000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_SURFACE                            ,         {2,      2500,    {4444,     5555,     800},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x60,    0x60,    0x80,        1,     10000,    0,        0,        240,    120,    30,         0,    {0,0,0},             0,       1,        0,        0,   255,  255,     7.0} },
    {LEVELID_JUNGLE                             ,        {10,      2500,    {1500,     2500,    1000},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x20,       0,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_TEMPLE                             ,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x28,        1,     10000,    0,        0,        160,    160,    190,        0,    {0,0,0},          -500,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_CAVERNS                            ,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -3000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CRADLE                             ,        {10,      9500,    {3333,     4444,     800},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    {0,0,0},        -10000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_SURFACE2                           ,         {2,      2000,    {2500,     3055,     750},        0,      0x3E7},    {0x3BD,    0x3E8},    {0x20,    0x10,    0x10,        1,      5000,    0,        0,         58,     17,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,    20.0} },
    {LEVELID_SURFACE2 + ENVIRONMENTDATA_CINEMA  ,         {2,      8000,    {6000,     8000,     800},        0,      0x3E7},    {0x3DF,    0x3E8},    {0x20,    0x10,    0x10,        1,      5000,    0,        0,         58,     17,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_BUNKER2                            ,        {10,     10000,    {1000,    15000,     750},        0,      0x3E7},    {0x3E4,    0x41A},    {0x10,       0,       0,        1,      5000,    0,        0,         58,     17,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_2,        {10,      2500,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_3,        {10,      2000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_BUNKER1 + ENVIRONMENTDATA_PLAYERS_4,        {10,      1500,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x80,    0x78,    0x70,        0,         0,    0,        0,        130,    120,    110,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_2 ,        {10,      7000,    {3333,     4444,     800},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    {0,0,0},        -10000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_3 ,        {10,      5500,    {3333,     4444,     800},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    {0,0,0},        -10000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CRADLE + ENVIRONMENTDATA_PLAYERS_4 ,        {10,      4000,    {3333,     4444,     800},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x60,    0x80,    0xA0,        1,      5000,    0,        0,        255,    255,      0,        0,    {0,0,0},        -10000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_2 ,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    {0,0,0},          -500,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_3 ,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    {0,0,0},          -500,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_TEMPLE + ENVIRONMENTDATA_PLAYERS_4 ,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x18,    0x18,        1,     10000,    0,        0,        120,    120,    120,        0,    {0,0,0},          -500,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_2,        {10,      7500,       {0,        0,       0},        0,      0x3E7},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -3000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_3,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -3000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CAVERNS + ENVIRONMENTDATA_PLAYERS_4,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3E1,    0x3E8},       {8,       0,       8,        0,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -3000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_2,        {10,      7500,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_3,        {10,      6000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_DEFAULT + ENVIRONMENTDATA_PLAYERS_4,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        0,         0,    0,        0,          0,      0,      0,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_CITADEL                            ,        {10,     20000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_2,        {10,     20000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_3,        {10,     20000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_CITADEL + ENVIRONMENTDATA_PLAYERS_4,        {10,     20000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x18,    0x50,    0x38,        1,      5000,    0,        0,        255,    108,      0,        0,    {0,0,0},         -1000,       1,        0,        0,   255,  255,     0.0} },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_2,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_3,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_COMPLEX + ENVIRONMENTDATA_PLAYERS_4,        {10,      5000,       {0,        0,       0},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x28,       0,       0,        1,     10000,    0,        0,        220,      0,     20,        0,    {0,0,0},             0,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_AZTEC                              ,        {10,     15000,    {3000,     4000,     600},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        1,      7500,    0,        0,         83,     72,     65,        0,    {0,0,0},         -5000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_AZTEC + ENVIRONMENTDATA_ALT        ,        {30,     15000,    {3000,     4000,     600},        0,      0x3E7},    {0x3E4,    0x3E8},       {0,       0,       0,        1,      7500,    0,        0,         83,     72,     65,        0,    {0,0,0},         -5000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_EGYPT                              ,        {10,     20000,    {3000,     4000,     600},        0,      0x3E7},    {0x3E4,    0x3E8},    {0x10,    0x30,    0x60,        1,      5000,    0,        0,        255,    255,    255,        0,    {0,0,0},         -5000,       0,        0,        0,     0,    0,     0.0} },
    {LEVELID_EGYPT + ENVIRONMENTDATA_ALT        ,        {10,     10000,    {3000,     4000,     600},        0,      0x3E7},    {0x3CC,    0x3E8},       {0,       0,       0,        1,      7500,    0,        0,         83,     10,     10,        0,    {0,0,0},         -5000,       0,        0,        0,     0,    0,     0.0} },
    {ENVIRONMENTDATA_END},
};
#endif


EnvironmentFoglessRecord fog_tables2[] = {
    {LEVELID_NONE   ,    0,       0x10,    0x40,    0,        5000.0,        0,        0,        255.0,        255.0,        255.0,        0,        {0,        0,        0},           0.0,        0,        0,          0.0,          0.0,          0.0,        0.0},
    {LEVELID_FRIGATE,    0x10,    0x30,    0x60,    1,        3000.0,        0,        0,        230.0,        230.0,        230.0,        1,        {0,        0,        0},        -150.0,        2,        0,        255.0,        255.0,        150.0,        0.0},
    {LEVELID_CUBA   ,    0x30,    0x40,    0x10,    0,        5000.0,        0,        0,        255.0,        255.0,        255.0,        0,        {0,        0,        0},           0.0,        0,        0,          0.0,          0.0,          0.0,        0.0},
    {ENVIRONMENTDATA_END}
};

// forward declarations

void fogLoadCurrentEnvironment(EnvironmentRecord *env);
void fogLoadFoglessCurrentEnvironment(EnvironmentFoglessRecord *env);

// end forward declarations


/**
* @return Pointer to Current Environment
 * canonically "bgFogGet"
 */
CurrentEnvironmentRecord *fogGetCurrentEnvironmentp(void)
{
    return &g_CurrentEnvironment;
}


f32 fogGetScaledFarFogIntensitySquared(void)
{
    return g_ScaledFarFogIntensity * g_ScaledFarFogIntensity;
}


/**
 * Co-op's "near" draw distance: the far plane comes in to this fraction of
 * the level's. Rooms and props past it aren't drawn, which saves time in both
 * views. The fog positions are fractions of the depth buffer range, which is
 * 1/z-like: the level's fog keeps starting at about the same distance and
 * still reaches full strength at the (nearer) far plane, so nothing pops.
 */
#define COOP_NEAR_DRAW_SCALE 0.5f

static EnvironmentRecord *s_FogLastEnvironment;

/**
 * Loads the last environment again, after the draw distance option changed.
 */
void fogReloadCurrentEnvironment(void)
{
    if (g_FogSkyIsEnabled && (s_FogLastEnvironment != NULL))
    {
        fogLoadCurrentEnvironment(s_FogLastEnvironment);
    }
}

void fogLoadCurrentEnvironment(EnvironmentRecord *env)
{
    f32 farfog;
    f32 zrange[2];
    f32 pk0;
    f32 pk4;
    f32 visScale;
    f32 fogScale;
    f32 fogOffset;

    s_FogLastEnvironment = env;
    farfog = env->Visibility.FarFog;

    if (g_CoopMode && optionsGetDrawNear())
    {
        farfog *= COOP_NEAR_DRAW_SCALE;
    }

    viSetZRange(env->Visibility.BlendMultiplier, farfog);
    viGetZRange(zrange);

    visScale = bgGetLevelVisibilityScale();
    zrange[0] /= visScale;
    zrange[1] /= visScale;

    g_DifferenceFromFarFogIntensity = ((f32) env->Fog.DifferenceFromFarIntensity / 1000.0f);
    g_FarFogIntensity = ((f32) env->Fog.FarIntensity / 1000.0f);

    g_ScaledFarFogIntensity = ((zrange[1] - zrange[0]) *  g_FarFogIntensity) + zrange[0];
    g_ScaledDifferenceFromFarFogIntensity = ((zrange[1] - zrange[0]) * g_DifferenceFromFarFogIntensity) + zrange[0];

    g_CurFogDetails.g_CurFogDetails = (env->Visibility.BlendMultiplier / visScale);
    pk0 = g_CurFogDetails.g_CurFogDetails;
    g_CurFogDetails.scaled_far_fog_dist = (farfog / visScale);
    pk4 = g_CurFogDetails.scaled_far_fog_dist;

    // numerator is constant 128.0f
    fogScale = (256.0f * (0.5f -         0.0f)) / (g_FarFogIntensity - g_DifferenceFromFarFogIntensity);
    fogOffset = (256.0f * (0.5f - g_DifferenceFromFarFogIntensity)) / (g_FarFogIntensity - g_DifferenceFromFarFogIntensity);

    g_CurFogDetails.far_fog_dist_scaled = ((pk4 * -fogScale * (pk0 + 1.0f)) / (pk4 - pk0)) / 255.0f;
    g_CurFogDetails.near_fog_dist_scaled = ((fogScale * (pk4 + 1.0f) / (pk4 - pk0)) + fogOffset) / 255.0f;

    g_CurrentEnvironment.DifferenceFromFarIntensity = (s32) env->Fog.DifferenceFromFarIntensity;
    g_CurrentEnvironment.FarIntensity = env->Fog.FarIntensity;
    g_CurrentEnvironment.Red = env->Sky.Red;
    g_CurrentEnvironment.Green = env->Sky.Green;
    g_CurrentEnvironment.Blue = env->Sky.Blue;
    g_CurrentEnvironment.Clouds = env->Sky.Clouds;
    g_CurrentEnvironment.CloudRepeat = env->Sky.CloudRepeat;
    g_CurrentEnvironment.SkyImageId = (s16) env->Sky.SkyImageId;
    g_CurrentEnvironment.CloudRed = env->Sky.CloudRed;
    g_CurrentEnvironment.CloudGreen = env->Sky.CloudGreen;
    g_CurrentEnvironment.CloudBlue = env->Sky.CloudBlue;
    g_CurrentEnvironment.IsWater = env->Sky.IsWater;
    g_CurrentEnvironment.WaterRepeat = env->Sky.WaterRepeat;
    g_CurrentEnvironment.WaterImageId = (s16) env->Sky.WaterImageId;
    g_CurrentEnvironment.WaterRed = env->Sky.WaterRed;
    g_CurrentEnvironment.WaterGreen = env->Sky.WaterGreen;
    g_CurrentEnvironment.WaterBlue = env->Sky.WaterBlue;
    g_CurrentEnvironment.WaterConcavity = env->Sky.WaterConcavity;

#if defined(VERSION_EU)
    #define FOG_ZERO 0
#else
    #define FOG_ZERO 0.0f
#endif

    if (env->Visibility.Nfd.NearFog == FOG_ZERO)
    {
        g_NearFogValuesP = NULL;
    }
    else
    {
#if defined(VERSION_EU)
        eu_loadCurrentNearFog.NearFog = env->Visibility.Nfd.NearFog;
        eu_loadCurrentNearFog.MaxVisRange = env->Visibility.Nfd.MaxVisRange;
        eu_loadCurrentNearFog.MaxObfuscationRange = env->Visibility.Nfd.MaxObfuscationRange;
        g_NearFogValuesP = &eu_loadCurrentNearFog;
#else
        g_NearFogValuesP = &env->Visibility.Nfd;
#endif
    }

#undef FOG_ZERO

    g_FogSkyIsEnabled = 1;
}

void fogLoadFoglessCurrentEnvironment(EnvironmentFoglessRecord *env)
{
    g_CurrentEnvironment.Red = env->Red;
    g_CurrentEnvironment.Green = env->Green;
    g_CurrentEnvironment.Blue = env->Blue;
    g_CurrentEnvironment.Clouds = env->Clouds;
    g_CurrentEnvironment.CloudRepeat = env->CloudRepeat;
    g_CurrentEnvironment.SkyImageId = env->SkyImageId;
    g_CurrentEnvironment.CloudRed = env->CloudRed;
    g_CurrentEnvironment.CloudGreen = env->CloudGreen;
    g_CurrentEnvironment.CloudBlue = env->CloudBlue;
    g_CurrentEnvironment.IsWater = env->IsWater;
    g_CurrentEnvironment.WaterRepeat = env->WaterRepeat;
    g_CurrentEnvironment.WaterImageId = env->WaterImageId;
    g_CurrentEnvironment.WaterRed = env->WaterRed;
    g_CurrentEnvironment.WaterGreen = env->WaterGreen;
    g_CurrentEnvironment.WaterBlue = env->WaterBlue;
    g_CurrentEnvironment.WaterConcavity = env->WaterConcavity;
}


void fogLoadLevelEnvironment(s32 level_id, s32 introCamera)
{
    EnvironmentRecord *env;
    EnvironmentFoglessRecord *fogless;
    EnvironmentFoglessRecord *found;
    s32 num_players;

    found = NULL;

    num_players = getPlayerCount();

    // Co-op plays solo levels, which only have solo fog tables.
    if ((num_players == 1) || !isMpGame())
    {
        num_players = 0;
    }

    g_ScaledFarFogIntensity = FLT_MAX;
    g_ScaledDifferenceFromFarFogIntensity = 0.0f;

    if (introCamera)
    {
        for (env = &fog_tables[0]; env->Id != 0; env++)
        {
            if (env->Id == (level_id + 900))
            {
                g_EnvironmentFoundp = env;
                g_EnvironmentMainp = env;
                g_EnvironmentAltp = env + 1;

                fogLoadCurrentEnvironment(g_EnvironmentFoundp);

                return;
            }
        }
    }

    for (env = &fog_tables[0]; env->Id != 0; env++)
    {
        if (env->Id == (level_id + (num_players * 100)))
        {
            g_EnvironmentFoundp = env;
            g_EnvironmentMainp = env;
            g_EnvironmentAltp = env + 1;

            fogLoadCurrentEnvironment(g_EnvironmentFoundp);

            return;
        }
    }

    if (num_players >= 2)
    {
        for (env = &fog_tables[0]; env->Id != 0; env++)
        {
            if (env->Id == (num_players * 100))
            {
                g_EnvironmentFoundp = env;
                g_EnvironmentMainp = env;
                g_EnvironmentAltp = env + 1;

                fogLoadCurrentEnvironment(g_EnvironmentFoundp);

                return;
            }
        }
    }

    viSetZRange(15.0f, 10000.0f);
    g_FogSkyIsEnabled = 0;

    for (fogless = fog_tables2; fogless->Id != 0; fogless++)
    {
        if (fogless->Id == level_id)
        {
            found = fogless;

            if(1)
            {
                // removed.
                // break or return maybe?
            }
        }
    }

    if (found == NULL)
    {
        found = &fog_tables2[0];
    }

    fogLoadFoglessCurrentEnvironment(found);
    g_EnvironmentFoundp = NULL;
}

/**
 * Switch to next Environment.
 * @param isTransition: Usually 0 for instant switch or 1 to transition gradually
 */
void fogSwitchToSolosky2(f32 frac)
{
    static EnvironmentRecord static_envr;

    static_envr = *g_EnvironmentMainp;

    static_envr.Visibility.BlendMultiplier =
        g_EnvironmentMainp->Visibility.BlendMultiplier
#if defined(VERSION_EU)
        + (frac * ((f32)g_EnvironmentAltp->Visibility.BlendMultiplier - (f32)g_EnvironmentMainp->Visibility.BlendMultiplier));
#else
        + (frac * (g_EnvironmentAltp->Visibility.BlendMultiplier - g_EnvironmentMainp->Visibility.BlendMultiplier));
#endif

    static_envr.Visibility.FarFog =
        g_EnvironmentMainp->Visibility.FarFog
#if defined(VERSION_EU)
        + (frac * ((f32)g_EnvironmentAltp->Visibility.FarFog - (f32)g_EnvironmentMainp->Visibility.FarFog));
#else
        + (frac * (g_EnvironmentAltp->Visibility.FarFog - g_EnvironmentMainp->Visibility.FarFog));
#endif

    static_envr.Fog.DifferenceFromFarIntensity =
        (f32)g_EnvironmentMainp->Fog.DifferenceFromFarIntensity
        + (frac * ((f32)g_EnvironmentAltp->Fog.DifferenceFromFarIntensity - (f32)g_EnvironmentMainp->Fog.DifferenceFromFarIntensity));

    static_envr.Fog.FarIntensity =
        (f32)g_EnvironmentMainp->Fog.FarIntensity
        + (frac * ((f32)g_EnvironmentAltp->Fog.FarIntensity - (f32)g_EnvironmentMainp->Fog.FarIntensity));

    static_envr.Sky.Red =
        (f32)g_EnvironmentMainp->Sky.Red
        + (frac * ((f32)g_EnvironmentAltp->Sky.Red - (f32)g_EnvironmentMainp->Sky.Red));

    static_envr.Sky.Green =
        (f32)g_EnvironmentMainp->Sky.Green
        + (frac * ((f32)g_EnvironmentAltp->Sky.Green - (f32)g_EnvironmentMainp->Sky.Green));

    static_envr.Sky.Blue =
        (f32)g_EnvironmentMainp->Sky.Blue
        + (frac * ((f32)g_EnvironmentAltp->Sky.Blue - (f32)g_EnvironmentMainp->Sky.Blue));

    static_envr.Sky.Red &= 0xf8;
    static_envr.Sky.Green &= 0xf8;
    static_envr.Sky.Blue &= 0xf8;

    fogLoadCurrentEnvironment(&static_envr);
}


/**
 * @param gdl:
 * @param arg1: not used?
*/
Gfx *fogSetRenderFogColor(Gfx *gdl)
{
    if (g_FogSkyIsEnabled == 0)
    {
        return gdl;
    }

    // The opaque and translucent passes use the same fog.
    gDPSetFogColor(gdl++, g_CurrentEnvironment.Red, g_CurrentEnvironment.Green, g_CurrentEnvironment.Blue, 0xff);
    gSPFogPosition(gdl++, g_CurrentEnvironment.DifferenceFromFarIntensity, g_CurrentEnvironment.FarIntensity);

    gSPSetGeometryMode(gdl++, G_FOG);
    gDPSetAlphaDither(gdl++, G_AD_NOISE);

    return gdl;
}


Gfx *fogRenderClearFogMode(Gfx *gdl)
{
    if (g_FogSkyIsEnabled == 0)
    {
        return gdl;
    }

    gSPClearGeometryMode(gdl++, G_FOG);

    return gdl;
}


s32 fogPositionIsVisibleThroughFog(coord3d *pos, f32 range)
{
    coord3d rel;
    f32 ff;
    coord3d *player_pos;
    Mtxf *player_mtx;

    if (g_FogSkyIsEnabled == 0)
    {
        return 1;
    }

    player_pos = playerGetPos();
    player_mtx = camGetWorldToScreenMtxf();

    rel.f[0] = pos->f[0] - player_pos->f[0];
    rel.f[1] = pos->f[1] - player_pos->f[1];
    rel.f[2] = pos->f[2] - player_pos->f[2];

    ff = (((rel.f[0] * player_mtx->m[0][0]) + (rel.f[1] * player_mtx->m[0][1]) + (rel.f[2] * player_mtx->m[0][2])));

    if (ff > (g_ScaledFarFogIntensity + range))
    {
        return 0;
    }

    return 1;
}


NearFogValues *fogGetNearFogValuesP(void)
{
    return g_NearFogValuesP;
}


s32 fogGetPropDistColor(PropRecord *prop, rgba_f32 *color)
{
    if (g_FogSkyIsEnabled == 0)
    {
        return 2; // No fog, props cannot be obscured by fog
    }

    if (prop->zDepth < 0.0f)
    {
        return 2; // Prop is behind the camera
    }

    color->rgba[0] = (f32) g_CurrentEnvironment.Red / 255.0f;
    color->rgba[1] = (f32) g_CurrentEnvironment.Green / 255.0f;
    color->rgba[2] = (f32) g_CurrentEnvironment.Blue / 255.0f;
    color->rgba[3] = (g_CurFogDetails.far_fog_dist_scaled / prop->zDepth) + g_CurFogDetails.near_fog_dist_scaled;

    if (color->rgba[3] < 0.0f)
    {
        return 2; // Prop has no fog effect coloring applied to it
    }

    if (color->rgba[3] > 1.0f)
    {
        return 0; // Prop is completely obscured by fog (don't render)
    }

    return 1; // Fog color will be applied to prop
}
