/**
 * HUD messages: the bottom-left messages ("Picked up a ...", objective updates) and the
 * top messages (characters talking to Bond), each a short queue shown for a few seconds,
 * plus the position readout of the "show position" debug cheat. See
 * docs/subsystems/player.md.
 */
#include "game/hud/hudmsg.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/menu/cheat.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/prop/alarm.h"
#include "game/text/language.h"
#include "game/text/text.h"
#include "game/world/bg.h"
#include <ultra64.h>
#include <bondtypes.h>
#include <boss.h>
#include <fr.h>
#include <math.h>
#include <str.h>

#ifdef VERSION_EU
    #define HUDMSG_BOTTOM_TIME_SKIP  0x1a
    #define HUDMSG_BOTTOM_TIME_SHORT 0x19
    #define HUDMSG_BOTTOM_TIME       0x64
    #define HUDMSG_TOP_TIME_SKIP     0x33
    #define HUDMSG_TOP_TIME_SHORT    0x32
    #define HUDMSG_TOP_TIME          0xc8
#else
    #define HUDMSG_BOTTOM_TIME_SKIP  0x1f
    #define HUDMSG_BOTTOM_TIME_SHORT 0x1e
    #define HUDMSG_BOTTOM_TIME       0x78
    #define HUDMSG_TOP_TIME_SKIP     0x3d
    #define HUDMSG_TOP_TIME_SHORT    0x3c
    #define HUDMSG_TOP_TIME          0xf0
#endif

// The font each bottom message is drawn in. PAL keeps one per message.
#if defined(VERSION_US)
    #define BOTTOM_FONT2(_index) s_BottomFont2
    #define BOTTOM_FONT1(_index) s_BottomFont1
#elif defined(VERSION_EU)
    #define BOTTOM_FONT2(_index) s_BottomFonts2[_index]
    #define BOTTOM_FONT1(_index) s_BottomFonts1[_index]
#endif

// How far above the bottom of the view the bottom message sits
#if defined(VERSION_US)
    #define BOTTOM_OFFSET_NO_AMMO 0x0C
    #define BOTTOM_OFFSET_AMMO    0x28
    #define BOTTOM_OFFSET_SPLIT   0x10
#elif defined(VERSION_EU)
    #define BOTTOM_OFFSET_NO_AMMO 0x16
    #define BOTTOM_OFFSET_AMMO    0x32
    #define BOTTOM_OFFSET_SPLIT   0x14
#endif

// Bottom messages: a queue of 5 in solo (s_BottomFirst is the one showing, s_BottomCount
// how many are queued); in multiplayer, one per player.
static char s_BottomText[5][HUDMSG_BOTTOM_LEN];
static s32 s_BottomFirst = 0;
static s32 s_BottomCount = 0;
#if defined(BUGFIX_R1)
static struct font *s_BottomFonts1[5];
static struct fontchar *s_BottomFonts2[5];
#endif
#ifdef BUGFIX_R0
static struct font *s_BottomFont1 = NULL;
static struct fontchar *s_BottomFont2 = NULL;
#endif

// Top messages: the one showing and the next.
static char s_TopText[2][HUDMSG_TOP_LEN];
static s32 s_TopFirst = 0;
static s32 s_TopCount = 0;
static s32 s_TopTimer = -1;
static s32 s_TopHidden = 0;   // PLAYERFLAG bits that hide the top messages

static DirectionLabels s_DebugCompassLabels = {{"n", "ne", "e", "se", "s", "sw", "w", "nw", "n"}};

void hudmsgResetIntroText(void)
{
    g_CurrentPlayer->hudmessoff = FALSE;
    g_CurrentPlayer->bondmesscnt = -1;
    s_BottomCount = 0;
    s_BottomFirst = 0;

#ifdef BUGFIX_R0
    s_BottomFont1 = g_FontBankGothic;
    s_BottomFont2 = g_FontBankGothicChars;
#endif
}


void hudmsgsSetOn(s32 flag)
{
    g_CurrentPlayer->hudmessoff &= ~flag;
}


void hudmsgsSetOff(s32 flags)
{
    g_CurrentPlayer->hudmessoff |= flags;
}


#ifdef VERSION_US
void hudmsgSetFonts(struct fontchar *font2, struct font *font1)
{
    s_BottomFont2 = font2;
    s_BottomFont1 = font1;
}
#endif


#ifdef BUGFIX_R1
void hudmsgBottomShow(char *string, struct fontchar *font, struct font *font1)
{
    s32 index;

    if (getPlayerCount() == 1)
    {
        // Solo: queue it behind the messages already showing
        if (s_BottomCount < 5)
        {
            index = (s_BottomFirst + s_BottomCount) % 5;
            strncpy(s_BottomText[index], string, HUDMSG_BOTTOM_LEN - 1);
            s_BottomText[index][HUDMSG_BOTTOM_LEN - 1] = 0;
            s_BottomFonts2[index] = font;
            s_BottomFonts1[index] = font1;
            s_BottomCount++;
        }
    }
    else
    {
        // Multiplayer: replaces the current player's message
        index = playerGetCurrentNum();
        strncpy(s_BottomText[index], string, HUDMSG_BOTTOM_LEN - 1);
        s_BottomText[index][HUDMSG_BOTTOM_LEN - 1] = 0;
        s_BottomFonts2[index] = font;
        s_BottomFonts1[index] = font1;
        g_CurrentPlayer->bondmesscnt = HUDMSG_BOTTOM_TIME;
    }
}

#else
void hudmsgBottomShow(char *mess)
{
    s32 index;

    if (getPlayerCount() == 1)
    {
        if (s_BottomCount < 5)
        {
            index = (s_BottomFirst + s_BottomCount) % 5;
            strncpy(s_BottomText[index], mess, HUDMSG_BOTTOM_LEN - 1);
            s_BottomCount++;
            s_BottomText[index][HUDMSG_BOTTOM_LEN - 1] = 0;
        }
    }
    else
    {
        index = playerGetCurrentNum();
        strncpy(s_BottomText[index], mess, HUDMSG_BOTTOM_LEN - 1);
        s_BottomText[index][HUDMSG_BOTTOM_LEN - 1] = 0;
        g_CurrentPlayer->bondmesscnt = HUDMSG_BOTTOM_TIME;
    }
}

#endif


#if defined(BUGFIX_R1)
void hudmsgBottomShowText(char *string)
{
    hudmsgBottomShow(string, g_FontBankGothicChars, g_FontBankGothic);
}
#endif


void hudmsgTickIntroText(void)
{
    if ((g_CurrentPlayer->hudmessoff == FALSE) && (g_CurrentPlayer->mpmenuon == FALSE))
    {
        if (g_CurrentPlayer->bondmesscnt >= 0)
        {
            g_CurrentPlayer->bondmesscnt -= g_ClockTimer;

            if (getPlayerCount() == 1)
            {
                if (g_CurrentPlayer->bondmesscnt < 0)
                {
                    s_BottomFirst = (s32) (s_BottomFirst + 1) % 5;
                    s_BottomCount = s_BottomCount - 1;
                }
                else if ((s_BottomCount >= 2) && (g_CurrentPlayer->bondmesscnt >= HUDMSG_BOTTOM_TIME_SKIP))
                {
                    g_CurrentPlayer->bondmesscnt = HUDMSG_BOTTOM_TIME_SHORT;
                }
            }
        }

        if ((getPlayerCount() == 1) && (g_CurrentPlayer->bondmesscnt < 0) && (s_BottomCount > 0))
        {
            if (s_BottomCount >= 2)
            {
                g_CurrentPlayer->bondmesscnt = HUDMSG_BOTTOM_TIME_SHORT;
            }
            else
            {
                g_CurrentPlayer->bondmesscnt = HUDMSG_BOTTOM_TIME;
            }
        }
    }
}


Gfx* hudmsgBottomRender(Gfx* gdl)
{
    s32 show;
    s32 view_left;
    s32 view_vert;
    s32 view_horiz;
    s32 view_top;
    s32 view_top_offset;
    s32 view_left_offset;

    if ((g_CurrentPlayer->hudmessoff == FALSE) && (g_CurrentPlayer->bondmesscnt >= 0) && (g_CurrentPlayer->mpmenuon == FALSE))
    {
        show = 0;
        if (getPlayerCount() == 1)
        {
            if ((u8) *s_BottomText[s_BottomFirst] != 0)
            {
                show = 1;
            }
        }
        else if (g_CurrentPlayer->bondmesscnt >= 0)
        {
            s_BottomFirst = playerGetCurrentNum();
            show = 1;
        }

        if (show != 0)
        {
            gdl = textSetupDraw(gdl);
            view_left_offset = 0;
            view_top_offset = 0;
            textMeasure(&view_top_offset, &view_left_offset ,s_BottomText[s_BottomFirst], BOTTOM_FONT2(s_BottomFirst), BOTTOM_FONT1(s_BottomFirst), 0);

            if (!viewIsHalfWidth())
            {
                view_left = viGetViewLeft() + 0x1E;
            }
            else if (coopPlayerPerson(playerGetCurrentNum()) & 1)
            {
                view_left = viGetViewLeft() + 0xA;
            }
            else
            {
                view_left = viGetViewLeft() + 0x1E;
            }

            view_horiz = view_left + view_left_offset;

            if (getPlayerCount() < 3)
            {
                if ((weaponGetAmmoType(gunGetWeapon(GUNLEFT)) == 0) && (countdownTimerIsVisible() == 0))
                {
                    view_top = (viGetViewTop() + viGetViewHeight()) - BOTTOM_OFFSET_NO_AMMO;
                }
                else
                {
                    view_top = (viGetViewTop() + viGetViewHeight()) - BOTTOM_OFFSET_AMMO;
                }
#if !defined(VERSION_EU)
                if (coopPlayerPerson(playerGetCurrentNum()) == 1)
                {
                    view_top -= 8;
                }
#endif
            }
            else
            {
                view_top = viGetViewTop()
                         + BOTTOM_OFFSET_SPLIT;
            }

            view_vert = view_top - view_top_offset;
            gdl = textRenderBlackBox(gdl, &view_left, &view_vert, &view_horiz, &view_top);
            gdl = textEndDraw(textRenderOutlined(gdl, &view_left, &view_vert, s_BottomText[s_BottomFirst], BOTTOM_FONT2(s_BottomFirst), BOTTOM_FONT1(s_BottomFirst), -1, 0x646464FFU, (s16) (s32) viGetX(), (s16) viGetY(), 0, 0));
        }
    }

    return gdl;
}


void hudmsgTopReset(void)
{
    s_TopTimer = -1;
    s_TopCount = 0;
    s_TopFirst = 0;
    s_TopHidden = 0;
}


void hudmsgTopClearFlag(int flag)
{
    s_TopHidden &= ~flag;
}


void hudmsgTopSetFlag(PLAYERFLAG flag)
{
    s_TopHidden |= flag;
}


/**
 * Co-op: the player each top message (a character speaking to them) shows
 * for. The top text is shared, so the other player's view skips it.
 */
static s8 s_CoopTopTextPlayer[2];

void hudmsgTopShow(char* mess)
{
    s32 index;
    if (s_TopCount >= 2) { return; }

    index = (s_TopFirst + s_TopCount) % 2;
    s_CoopTopTextPlayer[index] = playerGetCurrentNum();
    strncpy(s_TopText[index], mess, (HUDMSG_TOP_LEN-1));
    s_TopCount += 1;
    s_TopText[index][(HUDMSG_TOP_LEN-1)] = 0;
}


void hudmsgTopTick(void)
{
    if ((s_TopHidden == FALSE) && (g_CurrentPlayer->mpmenuon == FALSE))
    {
        if (s_TopTimer >= 0)
        {
            s_TopTimer -= g_ClockTimer;

            if (s_TopTimer < 0)
            {
                s_TopFirst = (s32) (s_TopFirst + 1) % 2;
                s_TopCount += -1;
            }
            else if ((s_TopCount >= 2) && (s_TopTimer >= HUDMSG_TOP_TIME_SKIP))
            {
                s_TopTimer = HUDMSG_TOP_TIME_SHORT;
            }
        }

        if ((s_TopTimer < 0) && (s_TopCount > 0))
        {
            if (s_TopCount >= 2)
            {
                s_TopTimer = HUDMSG_TOP_TIME_SHORT;
            }
            else
            {
                s_TopTimer = HUDMSG_TOP_TIME;
            }
        }
    }

}


Gfx *hudmsgTopRender(Gfx *gdl)
{
    TopMessageLocals msg;
    DebugTextBuffers debugtext;
    s32 debug_x;
    s32 debug_y;
    f32 theta_x;
    s32 debug_boxbottom;
    char *roomid;
    s32 debug_angle;
    DirectionLabels directions;

    struct
    {
        s16 screenwidth;
        s16 pad;
    } sw;

    if (s_TopHidden == 0)
    {
        if (s_TopTimer >= 0)
        {
            if (s_TopText[s_TopFirst][0] != '\0')
            {
                if ((g_CurrentPlayer->mpmenuon == 0)
                    && (!g_CoopMode || (s_CoopTopTextPlayer[s_TopFirst] == playerGetCurrentNum())))
                {
                    gdl = textSetupDraw(gdl);
                    msg.textwidth = 0;
                    msg.textheight = 0;
                    textMeasure(&msg.textheight, &msg.textwidth, s_TopText[s_TopFirst], g_FontZurichChars, g_FontZurich, 0);
                    if (cameraBufferToggle != 0)
                    {
                        msg.x = viGetViewLeft() + 0x46;
                        msg.y = viGetViewTop() + 0x10;
                        msg.y += 0x10;
                        msg.y = msg.y / 11;
                        msg.y *= 11;
                        msg.y -= 2;
                    }
                    else
                    {
                        msg.x = viGetViewLeft() + 0x1e;
#ifdef VERSION_EU
                        msg.y = viGetViewTop() + 0x10;
#else
                        msg.y = viGetViewTop() + 0xd;
#endif
                    }

                    msg.bottom = msg.y + msg.textheight;
                    gdl = textRenderRect(gdl, 0, msg.y - 2, viGetX(), msg.bottom, 0x64);
                    sw.screenwidth = viGetX();
                    gdl = textRender(gdl, &msg.x, &msg.y, s_TopText[s_TopFirst], g_FontZurichChars, g_FontZurich, -1, sw.screenwidth, viGetY(), 0, 0);
                    gdl = textEndDraw(gdl);
                    goto end;
                }
            }
        }
    }

    if (g_CheatShowPos)
    {
        theta_x = g_CurrentPlayer->bond.theta_transform.x;
        debug_angle = (s32) ((mathAtan2(-theta_x, g_CurrentPlayer->bond.theta_transform.z) * 180.0f) / M_PI_F);
        directions = s_DebugCompassLabels;
        roomid = bgDebPrintROOMID(g_CurrentPlayer->bond.current_tile_ptr->room);

        sprintf(debugtext.room, "%8s", roomid);
        sprintf(debugtext.x, "x %4.0f", g_CurrentPlayer->bond.collision_position.x);
        sprintf(debugtext.y, "y %4.0f", g_CurrentPlayer->bond.collision_position.y);
        sprintf(debugtext.z, "z %4.0f", g_CurrentPlayer->bond.collision_position.z);
        sprintf(debugtext.angle, "%s %3d", ((char *) (&directions)) + (((debug_angle + 0x16) / 0x2d) * 3), debug_angle);

        debug_x = viGetViewLeft() + 0x11;
        debug_y = viGetViewTop() + 0x11;
        debug_boxbottom = debug_y + 0xa;
        gdl = textSetupDraw(gdl);
        gdl = textRenderRect(gdl, 0, debug_y, viGetX(), debug_boxbottom + 1, 0x64);
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.room, g_FontBankGothicChars, g_FontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0x57;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.x, g_FontBankGothicChars, g_FontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0x8d;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.y, g_FontBankGothicChars, g_FontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0xc3;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.z, g_FontBankGothicChars, g_FontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        debug_x = viGetViewLeft() + 0xf9;
        sw.screenwidth = viGetX();
        gdl = textRender(gdl, &debug_x, &debug_y, debugtext.angle, g_FontBankGothicChars, g_FontBankGothic, -1, sw.screenwidth, viGetY(), 0, 0);
        gdl = textEndDraw(gdl);
    }

    end:
    return gdl;
}
