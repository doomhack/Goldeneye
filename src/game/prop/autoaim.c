/**
 * Auto-aim: picking which guard the player's aim is pulled towards.
 *
 * Each frame autoaimUpdateTarget scores the armed characters on screen, nearest first,
 * and the best one becomes the auto-aim target (playerUpdateAutoAimX/Y). A character
 * scores only when the middle band of the screen overlaps it, and the player can see the
 * floor it stands on. The score is 1 when the centre of the screen is over it, and falls
 * off with the distance from the centre.
 */

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/prop/autoaim.h"
#include "game/prop/prop.h"
#include "game/chr/chr.h"
#include "game/math/round.h"
#include "game/nav/stan.h"
#include "game/player/gun.h"
#include "game/player/player.h"

/* Set by difficulty: scales how far from the centre of the screen a target may be */
f32 g_AutoAimTolerance = 1.0;

static coord2d g_DefaultAutoAimCoord = {0};

/* The part of the screen auto-aim works in: the middle 65% vertically and 50% across */
#define AUTOAIM_TOP    0.175f
#define AUTOAIM_BOTTOM 0.825f
#define AUTOAIM_LEFT   0.25f
#define AUTOAIM_RIGHT  0.75f


/* The screen position of a view-space point */
static void autoaimProject(f32 x, f32 y, f32 z, coord2d *out)
{
    coord3d pos;

    pos.x = x;
    pos.y = y;
    pos.z = z;
    camViewToScreen(&pos, out);
}


/* Clamp a value to a range */
static f32 autoaimClamp(f32 value, f32 min, f32 max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}


/**
 * Score a target for auto-aim: 1 is best, and -2 means it can't be targeted.
 *
 * aimpos is the target's centre in view space, and xbounds and ybounds its view-space
 * x and y ranges (min, max). out_screen gets the screen point to aim at: the target's
 * centre, kept inside the auto-aim area.
 */
f32 autoaimScoreProp(PropRecord *targetprop, coord3d *aimpos, f32 *xbounds, f32 *ybounds, coord2d *out_screen)
{
    f32 top = camGetScreenTop() + camGetScreenHeight() * AUTOAIM_TOP;
    f32 bottom = camGetScreenTop() + camGetScreenHeight() * AUTOAIM_BOTTOM;
    f32 left = camGetScreenLeft() + camGetScreenWidth() * AUTOAIM_LEFT;
    f32 right = camGetScreenLeft() + camGetScreenWidth() * AUTOAIM_RIGHT;
    f32 score = -2.0f;
    f32 tolerance = 0.0f;
    coord2d aim;
    coord2d leftedge;
    coord2d rightedge;
    coord2d topedge;
    coord2d bottomedge;

    camViewToScreen(aimpos, &aim);
    autoaimProject(xbounds[0], aimpos->y, aimpos->z, &leftedge);
    autoaimProject(xbounds[1], aimpos->y, aimpos->z, &rightedge);
    autoaimProject(aimpos->x, ybounds[1], aimpos->z, &topedge);
    autoaimProject(aimpos->x, ybounds[0], aimpos->z, &bottomedge);

    if (!(bottomedge.y >= top && bottom >= topedge.y))
    {
        return score;
    }

    bool inrange = FALSE;
    f32 crosshairx;
    f32 crosshairy;

    gunGetCrosshairPos(&crosshairx, &crosshairy);
    leftedge.x = mathFloor(leftedge.x);
    rightedge.x = mathCeil(rightedge.x);

    if (currentPlayerGetXAutoAimEnabledRedirect())
    {
        // Horizontal auto-aim: the target's centre must be within 1.5 times its width
        // (scaled by difficulty) of the screen's centre, and inside the auto-aim area
        if (leftedge.x <= right && left <= rightedge.x)
        {
            tolerance = (rightedge.x - leftedge.x) * 1.5f;

            if (!isMpGame())
            {
                tolerance = tolerance * g_AutoAimTolerance;
            }

            inrange = camGetScreenLeft() + 0.5f * camGetScreenWidth() >= (leftedge.x + rightedge.x) * 0.5f - tolerance
                && camGetScreenLeft() + 0.5f * camGetScreenWidth() <= (leftedge.x + rightedge.x) * 0.5f + tolerance
                && left <= aim.x
                && right >= aim.x;
        }
    }
    else
    {
        // Otherwise the crosshair must be over the target
        inrange = leftedge.x <= crosshairx && crosshairx <= rightedge.x;
    }

    if (!inrange)
    {
        return score;
    }

    // The player must be able to see the floor the target stands on
    PropRecord *playerprop = currentPlayerGetProp();
    StandTile *stan = playerprop->stan;
    f32 eyeheight = playerGetEyeHeight(g_CurrentPlayer);

    playerSetMoving(playerprop, FALSE);

    if (stanTestLineUnobstructed(&stan, playerprop->pos.f[0], playerprop->pos.f[2], targetprop->pos.f[0], targetprop->pos.f[2],
                                 CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER, eyeheight, eyeheight, 0.0f, 1.0f)
        && stan == targetprop->stan)
    {
        out_screen->y = autoaimClamp(aim.y, top, bottom);

        if (currentPlayerGetXAutoAimEnabledRedirect())
        {
            out_screen->x = autoaimClamp(aim.x, left, right);
        }

        if (camGetScreenLeft() + 0.5f * camGetScreenWidth() >= leftedge.x && camGetScreenLeft() + 0.5f * camGetScreenWidth() <= rightedge.x)
        {
            // The centre of the screen is over the target: as good as it gets
            score = 1.0f;
        }
        else if (camGetScreenLeft() + 0.5f * camGetScreenWidth() >= leftedge.x)
        {
            // Left of the centre
            score = 1.0f - ((camGetScreenLeft() + 0.5f * camGetScreenWidth()) - rightedge.x) / tolerance;
        }
        else
        {
            // Right of the centre
            score = 1.0f - (leftedge.x - (camGetScreenLeft() + 0.5f * camGetScreenWidth())) / tolerance;
        }
    }

    playerSetMoving(playerprop, TRUE);

    return score;
}


/* Whether auto-aim may pick this prop: an armed guard, or in multiplayer and counter-op
   another player. The guard player in counter-op only aims at Bond. */
static bool autoaimIsCandidate(PropRecord *prop)
{
    if (prop->type != PROP_TYPE_CHR
        && (prop->type != PROP_TYPE_VIEWER || prop->obj == NULL || playerGetIndexForProp(prop) == playerGetCurrentNum()))
    {
        return FALSE;
    }

    // In co-op the other player is a friend
    if (g_CoopMode && !g_CounterOpMode && prop->type == PROP_TYPE_VIEWER)
    {
        return FALSE;
    }

    if (counterOpIsGuardPlayer(playerGetCurrentNum()) && prop->type != PROP_TYPE_VIEWER)
    {
        return FALSE;
    }

    ChrRecord *chr = prop->chr;

    return chrGetEquippedWeaponProp(chr, GUNRIGHT) != NULL || chrGetEquippedWeaponProp(chr, GUNLEFT) != NULL;
}


/**
 * Pick the auto-aim target from the props on screen, nearest first, and pass it to the
 * player with where it is on screen (-1 to 1 across and down).
 */
void autoaimUpdateTarget(void)
{
    PropRecord *bestprop = NULL;
    coord2d bestaim = g_DefaultAutoAimCoord;

    if (currentPlayerGetYAutoAimEnabledRedirect())
    {
        f32 bestscore = -1.0f;

        for (PropRecord **pp = g_LastOnScreenProp - 1; pp >= &g_OnScreenPropList[0]; pp--)
        {
            PropRecord *prop = *pp;
            coord3d aimpos;
            coord2d xbounds;
            coord2d ybounds;
            coord2d aim;

            if (prop == NULL || !autoaimIsCandidate(prop))
            {
                continue;
            }

            if (!chrGetOnscreenRenderBounds(prop, &aimpos, &xbounds, &ybounds))
            {
                continue;
            }

            f32 score = autoaimScoreProp(prop, &aimpos, &xbounds.x, &ybounds.x, &aim);

            if (bestscore < score)
            {
                bestscore = score;
                bestprop = prop;
                bestaim.x = aim.x;
                bestaim.y = aim.y;

                if (1.0f <= score)
                {
                    break;
                }
            }
        }
    }

    if (bestprop != NULL)
    {
        playerUpdateAutoAimY(bestprop, ((bestaim.y - camGetScreenTop()) / (camGetScreenHeight() * 0.5f)) - 1.0f);

        if (currentPlayerGetXAutoAimEnabledRedirect())
        {
            playerUpdateAutoAimX(bestprop, ((bestaim.x - camGetScreenLeft()) / (camGetScreenWidth() * 0.5f)) - 1.0f);
        }
    }
    else
    {
        playerUpdateAutoAimY(NULL, 0.0f);
        playerUpdateAutoAimX(NULL, 0.0f);
    }
}
