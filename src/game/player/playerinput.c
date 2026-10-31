/**
 * Player input: reading the controller each frame (playerProcessInput) for the
 * chosen control style, auto-aim (playerUpdateAutoAimX/Y), and the look and walk
 * speeds. See docs/subsystems/player.md.
 */
#include <fr.h>
#include <joy.h>
#include "game/chr/chraction.h"
#include "game/level/lv.h"
#include "game/math/trig.h"
#include "game/menu/front.h"
#include "game/menu/mpmenu.h"
#include "game/menu/watchmenu.h"
#include "game/nav/stan.h"
#include "game/player/inventory.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/player/playertune.h"
#include "game/prop/propobj.h"
#include "game/world/bgfog.h"
#include "os_extension.h"
#include "game/player/playerinput.h"


void currentPlayerSetLookAheadSetting(bool enabled)
{
    g_CurrentPlayer->automovecentreenabled = enabled;
}


void currentPlayerSetYAutoAimEnabled(bool enabled)
{
  g_CurrentPlayer->autoyaimenabled = enabled;
}


bool currentPlayerGetYAutoAimEnabled(void)
{
    if (!isMpGame())
    {
        return g_CurrentPlayer->autoyaimenabled;
    }

    return (bool) g_playerPerm->autoaim;
}


bool currentPlayerGetYAutoAimEnabledRedirect(void)
{
    return currentPlayerGetYAutoAimEnabled();
}


bool currentPlayerGetIsAiming(void)
{
  return g_CurrentPlayer->insightaimmode;
}


/**
 * Updates autoyaimtime60 by g_ClockTimer.
 * Will update player->autoaimy if new autoyaimtime60 < 0 or autoaim_target != g_CurrentPlayer->autoaim_target_y.
 */
void playerUpdateAutoAimY(struct PropRecord *autoaim_target, f32 auto_aim_y)
{
    if (g_CurrentPlayer->autoyaimtime60 >= 0)
    {
        g_CurrentPlayer->autoyaimtime60 = g_CurrentPlayer->autoyaimtime60 - g_ClockTimer;
    }

    if (autoaim_target != g_CurrentPlayer->autoaim_target_y)
    {
        if (g_CurrentPlayer->autoyaimtime60 < 0)
        {
            g_CurrentPlayer->autoyaimtime60 = BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_y = autoaim_target;
        }
        else
        {
            return;
        }
    }

    g_CurrentPlayer->autoaimy = auto_aim_y;
}


void currentPlayerSetXAutoAimEnabled(bool enabled)
{
  g_CurrentPlayer->autoxaimenabled = enabled;

  return;
}


bool currentPlayerGetXAutoAimEnabled(void)
{
    if (!isMpGame())
    {
        return g_CurrentPlayer->autoxaimenabled;
    }

    return (bool) g_playerPerm->autoaim;
}


bool currentPlayerGetXAutoAimEnabledRedirect(void)
{
    return currentPlayerGetXAutoAimEnabled();
}


/**
 * Updates autoxaimtime60 by g_ClockTimer.
 * Will update player->autoaimx if new autoxaimtime60 < 0 or autoaim_target_x != g_CurrentPlayer->autoaim_target_x.
 */
void playerUpdateAutoAimX(struct PropRecord *autoaim_target, f32 auto_aim_x)
{
    if (g_CurrentPlayer->autoxaimtime60 >= 0)
    {
        g_CurrentPlayer->autoxaimtime60 = g_CurrentPlayer->autoxaimtime60 - g_ClockTimer;
    }

    if (autoaim_target != g_CurrentPlayer->autoaim_target_x)
    {
        if (g_CurrentPlayer->autoxaimtime60 < 0)
        {
            g_CurrentPlayer->autoxaimtime60 = BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_x = autoaim_target;
        }
        else
        {
            return;
        }
    }

    g_CurrentPlayer->autoaimx = auto_aim_x;

}


/**
 * Perfect Dark bwalkUpdateSpeedSideways.
*/
void playerUpdateSpeedSideways(s32 dir) {
    if (dir == -1) {
        g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe - g_GlobalTimerDelta);
        if (g_CurrentPlayer->speedstrafe < -1.0f) {
            g_CurrentPlayer->speedstrafe = -1.0f;
        }
    }
    else {
        if (dir == 1) {
            g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe + g_GlobalTimerDelta);
            if (1.0f < g_CurrentPlayer->speedstrafe) {
                g_CurrentPlayer->speedstrafe = 1.0f;
            }
        } else {
            if (0.0f < g_CurrentPlayer->speedstrafe) {
                g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe - g_GlobalTimerDelta);
                if (g_CurrentPlayer->speedstrafe < 0.0f) {
                    g_CurrentPlayer->speedstrafe = 0.0f;
                }
            } else {
                g_CurrentPlayer->speedstrafe = (g_CurrentPlayer->speedstrafe + g_GlobalTimerDelta);
                if (0.0f < g_CurrentPlayer->speedstrafe) {
                    g_CurrentPlayer->speedstrafe = 0.0f;
                }
            }
        }
    }
    g_CurrentPlayer->speedsideways = g_CurrentPlayer->speedstrafe;
}

/**
 * Perfect Dark bwalkUpdateSpeedForwards.
*/
void playerUpdateSpeedForwards(s32 dir) {
    if (dir == 1) {
        g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo + g_GlobalTimerDelta);
        if (1.0f < g_CurrentPlayer->speedgo) {
            g_CurrentPlayer->speedgo = 1.0f;
        }
    } else {
        if (dir == -1) {
            g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo - g_GlobalTimerDelta);
            if (g_CurrentPlayer->speedgo < -1.0f) {
                g_CurrentPlayer->speedgo = -1.0f;
            }
        } else {
            if (0.0f < g_CurrentPlayer->speedgo) {
                g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo - g_GlobalTimerDelta);
                if (g_CurrentPlayer->speedgo < 0.0f) {
                    g_CurrentPlayer->speedgo = 0.0f;
                }
            } else {
                g_CurrentPlayer->speedgo = (g_CurrentPlayer->speedgo + g_GlobalTimerDelta);
                if (0.0f < g_CurrentPlayer->speedgo) {
                    g_CurrentPlayer->speedgo = 0.0f;
                }
            }
        }
    }
    g_CurrentPlayer->speedforwards = g_CurrentPlayer->speedgo;
}

/**
 * Duplicate of playerScaleLookSpeedTheta.
*/
f32 playerScaleLookSpeedVerta(f32 value) {
    if (value > 0) {
        return (viGetFovY() * value * -0.7f) / FOV_Y_F;
    }

    if (value < 0) {
        return (viGetFovY() * -value * 0.7f) / FOV_Y_F;
    }

    return 0;
}


// Eases the look up/down speed towards value (from the stick), scaled by the field of view
void playerUpdateSpeedVerta(f32 value)
{
    f32 mult = viGetFovY() / FOV_Y_F;
    f32 limit = playerScaleLookSpeedVerta(value);

    if (value > 0.0f)
    {
        if (g_CurrentPlayer->speedverta > 0.0f)
        {
            g_CurrentPlayer->speedverta -= (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedverta -= (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedverta < limit)
        {
            g_CurrentPlayer->speedverta = limit;
        }
    }
    else if (value < 0.0f)
    {
        if (g_CurrentPlayer->speedverta < 0.0f)
        {
            g_CurrentPlayer->speedverta += (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedverta += (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedverta > limit)
        {
            g_CurrentPlayer->speedverta = limit;
        }
    }
    else
    {
        if (g_CurrentPlayer->speedverta > limit)
        {
            g_CurrentPlayer->speedverta -= (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedverta < limit)
            {
                g_CurrentPlayer->speedverta = limit;
            }
        }
        else
        {
            g_CurrentPlayer->speedverta += (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedverta > limit)
            {
                g_CurrentPlayer->speedverta = limit;
            }
        }
    }
}


/**
 * Duplicate of playerScaleLookSpeedVerta.
*/
f32 playerScaleLookSpeedTheta(f32 speed) {
    if (0.0f < speed) {
        return (viGetFovY() * speed * -0.7f) / FOV_Y_F;
    } else if (speed < 0.0f) {
        return (viGetFovY() * -speed * 0.7f) / FOV_Y_F;
    } else {
        return 0.0f;
    }
}

// Eases the turn speed towards value (from the stick), scaled by the field of view
void playerUpdateSpeedTheta(f32 value)
{
    f32 mult = viGetFovY() / FOV_Y_F;
    f32 limit = playerScaleLookSpeedTheta(value);

    if (value > 0.0f)
    {
        if (g_CurrentPlayer->speedtheta > 0.0f)
        {
            g_CurrentPlayer->speedtheta -= (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedtheta -= (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (g_CurrentPlayer->speedtheta < limit)
        {
            g_CurrentPlayer->speedtheta = limit;
        }
    }
    else if (value < 0.0f)
    {
        if (g_CurrentPlayer->speedtheta < 0)
        {
            g_CurrentPlayer->speedtheta += (0.05f * g_GlobalTimerDelta * mult);
        }
        else
        {
            g_CurrentPlayer->speedtheta += (0.0125f * g_GlobalTimerDelta * mult);
        }

        if (limit < g_CurrentPlayer->speedtheta)
        {
            g_CurrentPlayer->speedtheta = limit;
        }
    }
    else
    {
        if (limit < g_CurrentPlayer->speedtheta)
        {
            g_CurrentPlayer->speedtheta -= (0.05f * g_GlobalTimerDelta * mult);

            if (g_CurrentPlayer->speedtheta < limit)
            {
                g_CurrentPlayer->speedtheta = limit;
            }
        }
        else
        {
            g_CurrentPlayer->speedtheta += (0.05f * g_GlobalTimerDelta * mult);

            if (limit < g_CurrentPlayer->speedtheta)
            {
                g_CurrentPlayer->speedtheta = limit;
            }
        }
    }
}


/**
 * Reads the current player's controller for this frame and turns it into
 * movement, look, aim, fire, reload, weapon switching, crouch, lean, the watch
 * and the tank's controls, for the chosen control style. Also runs auto-aim.
 */
void playerProcessInput(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons)
{
    struct MoveData moveData;

    s8 c2stickx;
    s8 c2sticky;
    u16 c2buttons;
    u16 c2oldbuttons;
    s32 adjustedStickX;
    s32 tmpc2sticky;
    s32 fireHeld;
    s32 firePressed;
    s32 aimHeld;
    s32 aimPressed;
    u16 shootButtons;
    u16 aimButtons;
    u16 invButtons;
    TankRecord *exitTank;
    s32 axis;
    TankRecord *enterTank;
    f32 accelScale;
    f32 noiseRadius;
    f32 zoomFov;
    f32 targetSpeed;
    f32 walkFrac;
    f32 unadjustedTargetSpeed;
    f32 dx;
    f32 dz;
    f32 dy;
    f32 targetPitch;
    StandTile *tile;
    f32 groundDist;
    f32 limit;
    coord3d lookPos;
    coord3d size;
    f32 fovScale;
    f32 pitchInput;
    f32 fovY;
    f32 turnScale;
    f32 turnInput;
    f32 turnTarget;
    s32 step;
    f32 swivelY;
    s32 canCycleWeapons;
    f32 swivelX;


    moveData.canSwivelGun = 0;
    moveData.canManualAim = 0;
    moveData.triggerOn = 0;
    moveData.btap = 0;
    moveData.canLookAhead = 0;
    moveData.canTurnTank = 0;
    moveData.canNaturalTurn = 0;
    moveData.canNaturalPitch = 0;
    moveData.digitalStepForward = 0;
    moveData.digitalStepBack = 0;
    moveData.digitalStepLeft = 0;
    moveData.digitalStepRight = 0;
    moveData.tankTurnLeftSpeed = 0;
    moveData.tankTurnRightSpeed = 0;
    moveData.speedVertaDown = 0;
    moveData.speedVertaUp = 0;
    moveData.aimTurnLeftSpeed = 0;
    moveData.aimTurnRightSpeed = 0;
    moveData.weaponBackOffset = 0;
    moveData.weaponForwardOffset = 0;
    moveData.aiming = 0;
    moveData.zooming = 0;
    moveData.zoomOutFovPersec = 0;
    moveData.zoomInFovPersec = 0;
    moveData.crouchDown = 0;
    moveData.crouchUp = 0;
    moveData.rLeanLeft = 0;
    moveData.rLeanRight = 0;
    moveData.detonating = 0;
    moveData.canAutoAim = 0;
    moveData.invertPitch = optionsGetLookInverted() == 0;
    moveData.disableLookAhead = 0;

    if (stick_x < -5) {
		moveData.controlStickXSafe = stick_x + 5;
	} else if (stick_x > 5) {
		moveData.controlStickXSafe = stick_x - 5;
	} else {
		moveData.controlStickXSafe = 0;
	}

	if (stick_y < -5) {
		moveData.controlStickYSafe = stick_y + 5;
	} else if (stick_y > 5) {
		moveData.controlStickYSafe = stick_y - 5;
	} else {
		moveData.controlStickYSafe = 0;
	}

    moveData.controlStickXRaw = (s32)stick_x;
    moveData.controlStickYRaw = (s32)stick_y;

    moveData.analogTurn = moveData.controlStickXSafe;
    moveData.analogPitch = moveData.controlStickYSafe;
    moveData.analogStrafe = moveData.controlStickXSafe;
    moveData.analogWalk = moveData.controlStickYSafe;

    if (g_CoopMode && g_CoopRespawnButtonsHeld[playerGetCurrentNum()])
    {
        if (buttons & START_BUTTON)
        {
            oldbuttons |= START_BUTTON;
        }
        else
        {
            g_CoopRespawnButtonsHeld[playerGetCurrentNum()] = FALSE;
        }
    }

    if (g_CurrentPlayer->bonddead == FALSE
        && g_bondviewForceDisarm <= 0
        && (
            (g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x5
                && ((buttons & ~oldbuttons) & START_BUTTON)
            )
            ||
            (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x5
                && g_CurrentPlayer->open_close_solo_watch_menu)
        )
        && !isMpGame()
        && !coopPartnerHasWatchOut())
    {
        // Co-op: either player's watch pauses the game for both and fills
        // the screen while it is out (see coopUpdateSoloView).
        watchTrigger(0);
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0
        && g_CurrentPlayer->bonddead == FALSE
        && (
            !isMpGame()
            || (
                g_MpStopPlay == 0
                && g_MpGameOver == 0)))
    {
        if (optionsGetControlStyle() == CONTROLLER_CONFIG_DOMINO /* 2.3 */
            || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODHEAD /* 2.4 */
            || optionsGetControlStyle() == CONTROLLER_CONFIG_GALORE /* 2.2 */
            || optionsGetControlStyle() == CONTROLLER_CONFIG_PLENTY /* 2.1 */
            )
        {
            c2stickx = joyGetStickX(playerGetCurrentNum() + getPlayerCount());
            c2sticky = joyGetStickY(playerGetCurrentNum() + getPlayerCount());
            c2buttons = joyGetButtons(playerGetCurrentNum() + getPlayerCount() , (u32)ANY_BUTTON);

            c2oldbuttons = g_CurrentPlayer->prev_buttons_pressed;

            if (c2stickx < -5)
            {
                adjustedStickX = c2stickx + 5;
            }
            else if (c2stickx > 5)
            {
                adjustedStickX = c2stickx - 5;
            }
            else
            {
                adjustedStickX = 0;
            }

            if (c2sticky < -5)
            {
                tmpc2sticky = c2sticky + 5;
            }
            else if (c2sticky >= 6)
            {
                tmpc2sticky = c2sticky - 5;
            }
            else
            {
                tmpc2sticky = 0;
            }

            /* 2.1 and 2.3 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_PLENTY || (optionsGetControlStyle() == CONTROLLER_CONFIG_DOMINO))
            {
                moveData.analogStrafe = adjustedStickX;
                moveData.analogPitch = tmpc2sticky;
            }
            else
            {
                if (g_PlayerIsInTank == 1 && !g_CurrentPlayer->insightaimmode)
                {
                    moveData.analogTurn = adjustedStickX;
                }
                else
                {
                    moveData.analogStrafe = adjustedStickX;
                }

                moveData.analogWalk = tmpc2sticky;
            }

            /* 2.1 and 2.2 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_PLENTY || optionsGetControlStyle() == CONTROLLER_CONFIG_GALORE)
            {
                aimHeld = (c2buttons & Z_TRIG) != 0;
                aimPressed = ((c2buttons & ~c2oldbuttons) & Z_TRIG) != 0;
                fireHeld = (buttons & Z_TRIG) != 0;
                firePressed = ((buttons & ~oldbuttons) & Z_TRIG) != 0;
            }
            else
            {
                aimHeld = (buttons & Z_TRIG) != 0;
                aimPressed = ((buttons & ~oldbuttons) & Z_TRIG) != 0;
                fireHeld = (c2buttons & Z_TRIG) != 0;
                firePressed = ((c2buttons & ~c2oldbuttons) & Z_TRIG) != 0;
            }

            if (lvGetControlsLocked() == 0 && mpMenuCanPlayerAct())
            {
                if (optionsGetAimControl() == 0)
                {
                    g_CurrentPlayer->insightaimmode = aimHeld;
                }
                else if (aimPressed)
                {
                    g_CurrentPlayer->insightaimmode = !g_CurrentPlayer->insightaimmode;
                }

                moveData.canSwivelGun = !g_CurrentPlayer->insightaimmode;
                moveData.canAutoAim = !g_CurrentPlayer->insightaimmode;

                moveData.canManualAim = g_CurrentPlayer->insightaimmode;

                moveData.btap = (
                    (((buttons & ~oldbuttons) & B_BUTTON) != 0)
                    ||
                    ((((c2buttons & ~c2oldbuttons) & B_BUTTON)) != 0)
                    );

                moveData.canLookAhead = !g_CurrentPlayer->insightaimmode;
                moveData.canTurnTank = 1;
                moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                moveData.canNaturalPitch = !g_CurrentPlayer->insightaimmode;

                if (g_CurrentPlayer->insightaimmode && (stick_y > 60))
                {
                    moveData.speedVertaDown = (f32) (stick_y - 60) / FLOAT_TEN_B;
                    if (moveData.speedVertaDown > 1.0f)
                    {
                        moveData.speedVertaDown = 1.0f;
                    }
                }
                else
                {
                    //moveData.speedVertaDown = 0;
                }

                if (g_CurrentPlayer->insightaimmode && (stick_y < -60))
                {
                    moveData.speedVertaUp = (f32) (-60 - stick_y) / FLOAT_TEN_B;
                    if (moveData.speedVertaUp > 1.0f)
                    {
                        moveData.speedVertaUp = 1.0f;
                    }
                }
                else
                {
                    //moveData.speedVertaUp = 0;
                }


                if (g_CurrentPlayer->insightaimmode && (stick_x < -60))
                {
                    moveData.aimTurnLeftSpeed = (f32) (-60 - stick_x) / FLOAT_TEN_B;
                    if (moveData.aimTurnLeftSpeed > 1.0f)
                    {
                        moveData.aimTurnLeftSpeed = 1.0f;
                    }
                }
                else
                {
                    //moveData.aimTurnLeftSpeed = 0;
                }

                if (g_CurrentPlayer->insightaimmode && (stick_x > 60) )
                {
                    moveData.aimTurnRightSpeed = (f32) (stick_x - 60) / FLOAT_TEN_B;
                    if (moveData.aimTurnRightSpeed > 1.0f)
                    {
                        moveData.aimTurnRightSpeed = 1.0f;
                    }
                }
                else
                {
                    //moveData.aimTurnRightSpeed = 0;
                }

                moveData.weaponBackOffset = (
                        ((buttons & A_BUTTON) != 0)
                        ||
                        ((c2buttons & A_BUTTON) != 0)
                    )
                    && (firePressed);

               moveData.weaponForwardOffset = (
                    (
                       (((buttons & ~oldbuttons) & A_BUTTON) != 0)
                       ||
                       (((c2buttons & ~c2oldbuttons) & A_BUTTON) != 0)
                    ))
                    && (!fireHeld);

                moveData.aiming = g_CurrentPlayer->insightaimmode;
                moveData.zooming = g_CurrentPlayer->insightaimmode;

                if ((weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH))
                    && g_CurrentPlayer->insightaimmode)
                {
                    if (tmpc2sticky < 0)
                    {
                        moveData.zoomOutFovPersec = (f32) -tmpc2sticky / 70.0f;
                        if (moveData.zoomOutFovPersec > 1.0f)
                        {
                            moveData.zoomOutFovPersec = 1.0f;
                        }

                        moveData.zoomOutFovPersec *= 2.0f;
                    }

                    if (tmpc2sticky > 0)
                    {
                        moveData.zoomInFovPersec = (f32) tmpc2sticky / 70.0f;
                        if (moveData.zoomInFovPersec > 1.0f)
                        {
                            moveData.zoomInFovPersec = 1.0f;
                        }

                        moveData.zoomInFovPersec *= 2.0f;
                    }
                }

                moveData.crouchDown = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                    && (g_CurrentPlayer->insightaimmode)
                    && (c2sticky < -30);

                moveData.crouchUp = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                    && (g_CurrentPlayer->insightaimmode)
                    && (c2sticky > 30);

                if ((
                           (((buttons & A_BUTTON) != 0) && (((buttons & ~oldbuttons) & B_BUTTON) != 0))
                        || (((buttons & B_BUTTON) != 0) && (((buttons & ~oldbuttons) & A_BUTTON) != 0))
                        || ((c2buttons & A_BUTTON) && ((c2buttons & ~c2oldbuttons) & B_BUTTON))
                        || ((c2buttons & B_BUTTON) && ((c2buttons & ~c2oldbuttons) & A_BUTTON)))
                    && (gunGetWeapon(GUNRIGHT) == ITEM_REMOTEMINE))
                {
                    moveData.detonating = 1;
                    moveData.weaponBackOffset = 0;
                    moveData.weaponForwardOffset = 0;
                    moveData.btap = 0;
                }

                if (g_PlayerIsInTank == 1 && g_CurrentPlayer->insightaimmode)
                {
                    if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
                    {
                        moveData.controlStickXRaw = 0;

                        if (moveData.analogStrafe == 0)
                        {
                            moveData.analogStrafe = moveData.analogTurn;
                        }
                    }
                    else if (moveData.analogStrafe == 0)
                    {
                        if (moveData.aimTurnLeftSpeed > 0)
                        {
                            moveData.tankTurnLeftSpeed = moveData.aimTurnLeftSpeed;
                        }

                        if (moveData.aimTurnRightSpeed > 0)
                        {
                            moveData.tankTurnRightSpeed = moveData.aimTurnRightSpeed;
                        }
                    }

                    moveData.aimTurnLeftSpeed = 0;
                    moveData.aimTurnRightSpeed = 0;
                }
            }

            moveData.triggerOn = (fireHeld)
                && (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
                && ((buttons & A_BUTTON) == 0)
                && ((c2buttons & A_BUTTON) == 0);

            moveData.disableLookAhead = 1;
            g_CurrentPlayer->prev_buttons_pressed = c2buttons;
        }
        else
        {
            /* 1.3 and 1.4 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_KISSY
                || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODNIGHT)
            {
                shootButtons = A_BUTTON;
                aimButtons = Z_TRIG;
                invButtons = L_TRIG | R_TRIG;
            }
            else
            {
                shootButtons = Z_TRIG;
                aimButtons = L_TRIG | R_TRIG;
                invButtons = A_BUTTON;
            }

            if (lvGetControlsLocked() == 0)
            {
                if (mpMenuCanPlayerAct())
                {
                    if (optionsGetAimControl() == 0)
                    {
                        g_CurrentPlayer->insightaimmode = (buttons & aimButtons) != 0;
                    }
                    else if ((buttons & ~oldbuttons) & aimButtons)
                    {
                        g_CurrentPlayer->insightaimmode = !g_CurrentPlayer->insightaimmode;
                    }

                    moveData.canSwivelGun = !g_CurrentPlayer->insightaimmode;
                    moveData.canAutoAim = !g_CurrentPlayer->insightaimmode;
                    moveData.btap = ((buttons & ~oldbuttons) & B_BUTTON) != 0;
                    moveData.canManualAim = g_CurrentPlayer->insightaimmode;

                    /* 1.2 and 1.4 */
                    if (optionsGetControlStyle() == CONTROLLER_CONFIG_SOLITARE
                        || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODNIGHT)
                    {
                        if ((buttons & (L_JPAD | L_CBUTTONS)) != 0)
                        {
                            if (!g_CurrentPlayer->insightaimmode)
                            {
                                if (g_PlayerIsInTank == 1)
                                {
                                    moveData.aimTurnLeftSpeed = 1.0f;
                                }
                                else
                                {
                                    moveData.digitalStepLeft = 1;
                                }
                            }
                            else
                            {
                                moveData.tankTurnLeftSpeed = 1.0f;
                            }
                        }

                        if ((buttons & (R_JPAD | R_CBUTTONS)) != 0)
                        {
                            if (!g_CurrentPlayer->insightaimmode)
                            {
                                if (g_PlayerIsInTank == 1)
                                {
                                    moveData.aimTurnRightSpeed = 1.0f;
                                }
                                else
                                {
                                    moveData.digitalStepRight = 1;
                                }
                            }
                            else
                            {
                                moveData.tankTurnRightSpeed = 1.0f;
                            }
                        }

                        moveData.digitalStepForward = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (U_JPAD | U_CBUTTONS)) );

                        moveData.digitalStepBack = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (D_JPAD | D_CBUTTONS)));

                        moveData.canNaturalPitch = !g_CurrentPlayer->insightaimmode;

                        if (g_PlayerIsInTank == 1)
                        {
                            moveData.canTurnTank = !g_CurrentPlayer->insightaimmode;
                        }
                        else
                        {
                            moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                        }
                    }
                    else
                    {
                        if ((buttons & (s32)(L_JPAD | L_CBUTTONS)) != 0)
                        {
                            moveData.tankTurnLeftSpeed = 1.0f;
                        }
                        /* optional else statement, matches with or without. */
                        else
                        {
                            moveData.tankTurnLeftSpeed = 0;
                        }

                        if ((buttons & (s32)(R_JPAD | R_CBUTTONS)) != 0)
                        {
                            moveData.tankTurnRightSpeed = 1.0f;
                        }
                        /* optional else statement, matches with or without. */
                        else
                        {
                            moveData.tankTurnRightSpeed = 0;
                        }

                        moveData.digitalStepLeft = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (s32)(L_JPAD | L_CBUTTONS)) );

                        moveData.digitalStepRight = (!g_CurrentPlayer->insightaimmode)
                            && ((buttons & (s32)(R_JPAD | R_CBUTTONS)));

                        moveData.canLookAhead = !g_CurrentPlayer->insightaimmode;

                        if ((!g_CurrentPlayer->insightaimmode) && (buttons & (U_JPAD | U_CBUTTONS)) )
                        {
                            moveData.speedVertaDown = 1.0f;
                        }

                        if ((!g_CurrentPlayer->insightaimmode) && (buttons & (D_JPAD | D_CBUTTONS)))
                        {
                            moveData.speedVertaUp = 1.0f;
                        }

                        moveData.canNaturalTurn = !g_CurrentPlayer->insightaimmode;
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_y > 60))
                    {
                        moveData.speedVertaDown = (f32) (stick_y - 60) / FLOAT_TEN_B;
                        if (moveData.speedVertaDown > 1.0f)
                        {
                            moveData.speedVertaDown = 1.0f;
                        }
                    }
                    else if ((g_CurrentPlayer->insightaimmode) && (stick_y < -60))
                    {
                        moveData.speedVertaUp = (f32) (-60 - stick_y) / FLOAT_TEN_B;
                        if (moveData.speedVertaUp > 1.0f)
                        {
                            moveData.speedVertaUp = 1.0f;
                        }
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_x < -60))
                    {
                        moveData.aimTurnLeftSpeed = (f32) (-60 - stick_x) / FLOAT_TEN_B;
                        if (moveData.aimTurnLeftSpeed > 1.0f)
                        {
                            moveData.aimTurnLeftSpeed = 1.0f;
                        }
                    }

                    if ((g_CurrentPlayer->insightaimmode) && (stick_x > 60))
                    {
                        moveData.aimTurnRightSpeed = (f32) (stick_x - 60) / FLOAT_TEN_B;
                        if (moveData.aimTurnRightSpeed > 1.0f)
                        {
                            moveData.aimTurnRightSpeed = 1.0f;
                        }
                    }

                    moveData.weaponBackOffset =
                        ((buttons & invButtons) != 0)
                        &&
                        (((buttons & ~oldbuttons) & shootButtons) != 0)
                        ;

                    moveData.weaponForwardOffset =
                        (((buttons & ~oldbuttons) & invButtons) != 0)
                        &&
                        ((buttons & shootButtons) == 0)
                        ;

                    moveData.aiming = g_CurrentPlayer->insightaimmode;
                    moveData.zooming = g_CurrentPlayer->insightaimmode;

                    if ((weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH))
                        && g_CurrentPlayer->insightaimmode
                        )
                    {
                        /* down = 0x404 */
                        if ((buttons & (D_JPAD | D_CBUTTONS)) != 0)
                        {
                            moveData.zoomOutFovPersec = 1.0f;
                        }

                        if ((buttons & (U_JPAD | U_CBUTTONS)) != 0)
                        {
                            moveData.zoomInFovPersec = 1.0f;
                        }
                    }

                    moveData.crouchDown = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                        && (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (D_JPAD | D_CBUTTONS)));

                    moveData.crouchUp = (weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0)
                        && (g_CurrentPlayer->insightaimmode)
                        && ((~buttons & (U_JPAD | U_CBUTTONS)));

                    moveData.rLeanLeft = (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (L_JPAD | L_CBUTTONS)));

                    moveData.rLeanRight = (g_CurrentPlayer->insightaimmode)
                        && ((buttons & (R_JPAD | R_CBUTTONS)));

                    if (
                        ((((buttons & invButtons) != 0) && (((buttons & ~oldbuttons) & B_BUTTON) != 0))
                            || ((buttons & B_BUTTON) && (((buttons & ~oldbuttons) & invButtons) != 0)))
                        && (gunGetWeapon(GUNRIGHT) == ITEM_REMOTEMINE))
                    {
                        moveData.detonating = 1;
                        moveData.weaponBackOffset = 0;
                        moveData.weaponForwardOffset = 0;
                        moveData.btap = 0;
                    }

                    if ((g_PlayerIsInTank == 1) && (g_CurrentPlayer->insightaimmode))
                    {
                        if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
                        {
                            moveData.controlStickXRaw = 0;
                            moveData.canTurnTank = 1;
                        }
                        else if ((moveData.tankTurnLeftSpeed == 0) && (moveData.tankTurnRightSpeed == 0))
                        {
                            if (moveData.aimTurnLeftSpeed > 0)
                            {
                                moveData.tankTurnLeftSpeed = moveData.aimTurnLeftSpeed;
                            }

                            if (moveData.aimTurnRightSpeed > 0)
                            {
                                moveData.tankTurnRightSpeed = moveData.aimTurnRightSpeed;
                            }
                        }

                        moveData.aimTurnLeftSpeed = 0;
                        moveData.aimTurnRightSpeed = 0;
                    }
                }
            }

            moveData.triggerOn = ((buttons & shootButtons)  != 0)
                && (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
                && ((buttons & invButtons) == 0);

            /* 1.2 and 1.4 */
            if (optionsGetControlStyle() == CONTROLLER_CONFIG_SOLITARE || optionsGetControlStyle() == CONTROLLER_CONFIG_GOODNIGHT)
            {
                moveData.disableLookAhead = 1;
            }
        }
    }

    g_CurrentPlayer->activatePressed = 0;

    if (moveData.btap)
    {
        /* If Bond is in the tank and pressed B, then exit. */
        if (g_PlayerIsInTank == 1)
        {
            exitTank = (struct TankRecord *)g_PlayerTankProp->obj;
            exitTank->storedShells = weaponGetHeldAmmo(ITEM_TANKSHELLS);

            weaponGiveAmmo(ITEM_TANKSHELLS, 0);
            invRemoveItemByID(ITEM_TANKSHELLS);

            if (gunGetWeapon(GUNRIGHT) == ITEM_TANKSHELLS)
            {
                exitTank->storedShells += gunGetMagazineAmmo(GUNRIGHT);
                gunSwitchWhenEmpty();
            }

            exitTank->is_firing_tank = 0;
            g_PlayerIsInTank = 0;
            g_CurrentPlayer->speedsideways = 0;
            g_CurrentPlayer->speedforwards = 0;
            g_CurrentPlayer->speedtheta = 0;

            for (axis=0; axis<3; axis++)
            {
                g_CurrentPlayer->bondshotspeed.f[axis] = 0;
            }

            g_CurrentPlayer->crouchpos = CROUCH_STAND;
        }
        /* If Bond is standing on the tank and pressed B, enter the tank. */
        else if (g_PlayerTankProp != NULL
            && g_PlayerTankProp->type == PROP_TYPE_OBJ
            && g_PlayerTankProp->obj->type == PROPDEF_TANK
            && g_BondCanEnterTank
            // Counter-op: the guard player can't drive the tank.
            && !counterOpIsGuardPlayer(playerGetCurrentNum()))
        {
            enterTank = (struct TankRecord *)g_PlayerTankProp->obj;

            invAddItem(ITEM_TANKSHELLS);
            weaponGiveAmmo(ITEM_TANKSHELLS, enterTank->storedShells);
            enterTank->storedShells = 0;
            g_TankTurretVerticalAngle = enterTank->turret_vertical_angle;
            g_TankTurretVerticalAngleRelated = g_TankTurretVerticalAngle / TANK_VERT_ANGLE_FACTOR;
            g_TankTurretAngle = enterTank->turret_orientation_angle;
            g_TankTurretOrientationAngleRad = enterTank->turret_orientation_angle;
            g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleRad / TANK_VERT_ANGLE_RAD_FACTOR;
            tank_turret_turn_speed = 0;
            g_TankOrientationAngle = enterTank->tank_orientation_angle;
            g_TankTurnSpeed = 0;
            g_PlayerIsInTank = 1;
            g_EnterTankAudioState = TANK_RUN_STATE_NOT_RUNNING;
            g_CurrentPlayer->speedsideways = 0;
            g_CurrentPlayer->speedforwards = 0;
            g_CurrentPlayer->speedtheta = 0;
            g_CurrentPlayer->crouchpos = CROUCH_HALF;
            g_TankEnteringSitHeight = 0;
            g_TankEnteringSitHeightRemain = 1.0f;
            g_TankEnterBondHorizAngleDeg = g_CurrentPlayer->vv_theta;
            g_TankEnterBondVertAngleDeg = g_CurrentPlayer->vv_verta;
            g_EnterTankCoord.f[0] = g_CurrentPlayer->bond.collision_position.f[0];
            g_EnterTankCoord.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            g_EnterTankCoord.f[2] = g_CurrentPlayer->bond.collision_position.f[2];
            g_TankDamagePenaltyTicks = 0;

            tankUpdateTurretOffset();
        }
        else
        {
            g_CurrentPlayer->activatePressed = 1;
        }
    }

    if (moveData.invertPitch == 0)
    {
        f32 tmp;

        moveData.controlStickYRaw = (s32) -stick_y;
        moveData.analogPitch = -moveData.analogPitch;
        tmp = moveData.speedVertaDown;
        moveData.speedVertaDown = moveData.speedVertaUp;
        moveData.speedVertaUp = tmp;
    }

    if (playerIsShowingDamage() && !isMpGame())
    {
        moveData.triggerOn = 0;
    }

    moveData.triggerOn = counterOpSuicidePillTick(moveData.triggerOn);

    gunTickGameplay(moveData.triggerOn);

    if (playerGetVisibleToGuards()
        && (gunGetFiringStatus(GUNRIGHT)
            || gunGetFiringStatus(GUNLEFT)))
    {
        noiseRadius = 0;

        if (gunGetFiringStatus(GUNRIGHT) && gunGetNoise(GUNRIGHT) > noiseRadius)
        {
            noiseRadius = gunGetNoise(GUNRIGHT);
        }

        if (gunGetFiringStatus(GUNLEFT) && noiseRadius < gunGetNoise(GUNLEFT))
        {
            noiseRadius = gunGetNoise(GUNLEFT);
        }

        chrCheckGuardsHeardSound(noiseRadius);
    }

    gunSetSightVisible(GUNSIGHTREASON_NOTAIMING, moveData.aiming);

    if (moveData.zoomOutFovPersec > 0)
    {
        gunZoomOut(moveData.zoomOutFovPersec);
    }

    if (moveData.zoomInFovPersec > 0)
    {
        gunZoomIn(moveData.zoomInFovPersec);
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        zoomFov = 60.0f;

        if (moveData.zooming)
        {
            zoomFov = gunGetZoomFov();

            if (zoomFov <= 0)
            {
                zoomFov = 60.0f;
            }
        }

        watchZoomTo(zoomFov);
        watchUpdateZoom();
    }

    if (g_PlayerIsInTank == 1)
    {
        g_TankTurretTurn = 0;

        if (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING)
        {
            if (moveData.tankTurnRightSpeed > 0)
            {
                g_TankTurretTurn += g_GlobalTimerDelta * moveData.tankTurnRightSpeed * DegToRad1Fact(1);
            }
            else if (moveData.tankTurnLeftSpeed > 0)
            {
                g_TankTurretTurn -= g_GlobalTimerDelta * moveData.tankTurnLeftSpeed * DegToRad1Fact(1);
            }
            else if (moveData.canTurnTank)
            {
                targetSpeed = (f32) moveData.analogStrafe / 70.0f;

                if (targetSpeed > 1.0f)
                {
                    targetSpeed = 1.0f;
                }

                if (targetSpeed < -1.0f)
                {
                    targetSpeed = -1.0f;
                }

                g_TankTurretTurn += DegToRad1Fact(1) * targetSpeed * g_GlobalTimerDelta;
            }

            if (!g_CurrentPlayer->insightaimmode)
            {
                walkFrac = 0;
                targetSpeed = 1.0f;
                accelScale = 1.0f;

                if (moveData.canLookAhead)
                {
                    walkFrac = (f32) moveData.analogWalk / 70.0f;
                }
                else if (moveData.digitalStepForward)
                {
                    walkFrac = 1.0f;
                }
                else if (moveData.digitalStepBack)
                {
                    walkFrac = -1.0f;
                }

                if (walkFrac > 1.0f)
                {
                    walkFrac = 1.0f;
                }
                else if (walkFrac < -1.0f)
                {
                    walkFrac = -1.0f;
                }

                unadjustedTargetSpeed = walkFrac * TANK_MAX_SPEED;
                targetSpeed = unadjustedTargetSpeed;

                if (g_TankDamagePenaltyTicks > 0)
                {
                    targetSpeed = unadjustedTargetSpeed * 0.5f;
                    accelScale = 4.0f;
                    g_TankDamagePenaltyTicks -= g_ClockTimer;
                }

                if (targetSpeed != g_CurrentPlayer->speedforwards)
                {
                    if (g_CurrentPlayer->speedforwards < targetSpeed)
                    {
                        unadjustedTargetSpeed = ((((((targetSpeed - g_CurrentPlayer->speedforwards) / 4.0f) / TANK_MAX_SPEED) + 0.5f) * accelScale * FLOAT_TEN_A) / 60.0f);

                        g_CurrentPlayer->speedforwards += (unadjustedTargetSpeed) * g_GlobalTimerDelta;

                        if (targetSpeed < g_CurrentPlayer->speedforwards)
                        {
                            g_CurrentPlayer->speedforwards = targetSpeed;
                        }
                    }
                    else if (targetSpeed < g_CurrentPlayer->speedforwards)
                    {
                        unadjustedTargetSpeed = ((((((g_CurrentPlayer->speedforwards - targetSpeed) / 4.0f) / TANK_MAX_SPEED) + 0.5f) * accelScale * -FLOAT_TEN_A) / 60.0f);

                        g_CurrentPlayer->speedforwards += (unadjustedTargetSpeed) * g_GlobalTimerDelta;

                        if (g_CurrentPlayer->speedforwards < targetSpeed)
                        {
                            g_CurrentPlayer->speedforwards = targetSpeed;
                        }
                    }
                }
            }
        }
    }
    else
    {
        if (moveData.digitalStepLeft)
        {
            playerUpdateSpeedSideways(-1);
        }
        else if (moveData.digitalStepRight)
        {
            playerUpdateSpeedSideways(1);
        }
        else
        {
            playerUpdateSpeedSideways(0);
        }

        if (moveData.canTurnTank) // ?? not sure why this tank property is used here. Is the name wrong?
        {
            g_CurrentPlayer->speedsideways = (f32) moveData.analogStrafe / 70.0f;
        }

        if (moveData.digitalStepForward)
        {
            playerUpdateSpeedForwards(1);
            g_CurrentPlayer->speedmaxtime60 += g_ClockTimer;
        }
        else if (moveData.digitalStepBack)
        {
            playerUpdateSpeedForwards(-1);
        }
        else
        {
            playerUpdateSpeedForwards(0);
        }

        if (moveData.canLookAhead)
        {
            g_CurrentPlayer->speedforwards = (f32) moveData.analogWalk / 70.0f;

            if (moveData.analogWalk > 60)
            {
                g_CurrentPlayer->speedmaxtime60 += g_ClockTimer;
            }
            else
            {
                g_CurrentPlayer->speedmaxtime60 = 0;
            }
        }

        if (g_CurrentPlayer->speedforwards > 1.0f)
        {
            g_CurrentPlayer->speedforwards = 1;
        }

        if (g_CurrentPlayer->speedforwards < -1.0f)
        {
            g_CurrentPlayer->speedforwards = -1.0f;
        }

        if (g_CurrentPlayer->speedsideways > 1)
        {
            g_CurrentPlayer->speedsideways = 1;
        }

        if (g_CurrentPlayer->speedsideways < -1)
        {
            g_CurrentPlayer->speedsideways = -1;
        }

        g_CurrentPlayer->speedforwards *= 1.08f;
        g_CurrentPlayer->speedforwards *= g_CurrentPlayer->speedboost;

        if ((moveData.canLookAhead == 0) && (moveData.digitalStepForward == 0))
        {
            g_CurrentPlayer->speedmaxtime60 = 0;
        }

        if (moveData.rLeanLeft)
        {
            playerSetLeanTarget(-1);
        }
        else if (moveData.rLeanRight)
        {
            playerSetLeanTarget(1);
        }
        else
        {
            playerSetLeanTarget(0);
        }

        if (moveData.crouchDown)
        {
            playerAdjustCrouch(-2);
        }
        else if (moveData.crouchUp)
        {
            playerAdjustCrouch(2);
        }
    }

    if (g_CurrentPlayer->speedmaxtime60 >= THREE_SECOND_TICKS)
    {
        if (g_CurrentPlayer->speedboost < SPEED_RUN_MAX)
        {
            g_CurrentPlayer->speedboost += (SPEED_TICK_ADJUST * g_GlobalTimerDelta);
        }

        if (g_CurrentPlayer->speedboost > SPEED_RUN_MAX)
        {
            g_CurrentPlayer->speedboost = SPEED_RUN_MAX;
        }
    }
    else
    {
        if (g_CurrentPlayer->speedboost > SPEED_REGULAR_MAX)
        {
            g_CurrentPlayer->speedboost -= (SPEED_TICK_ADJUST * g_GlobalTimerDelta);
        }

        if (g_CurrentPlayer->speedboost < SPEED_REGULAR_MAX)
        {
            g_CurrentPlayer->speedboost = SPEED_REGULAR_MAX;
        }
    }

    if (g_CurrentPlayer->watch_animation_state == WATCH_ANIMATION_0x0)
    {
        // By default the camera is pitched slightly below the horizon
        targetPitch = -4.0f;

        // lookaheadcentreenabled is always true, so this block always executes.
        if (g_CurrentPlayer->lookaheadcentreenabled)
        {
            tile = g_CurrentPlayer->bond.current_tile_ptr;
            groundDist = 300.0f;

            // prop, f32 *collision_radius, f32 *height, f32 *always_30
            playerGetCollisionSize(g_CurrentPlayer->prop, &size.f[0], &size.f[2], &size.f[1]);

            lookPos.f[0] = g_CurrentPlayer->bond.collision_position.f[0] + (g_CurrentPlayer->bond.theta_transform.f[0] * 300.0f);
            lookPos.f[1] = g_CurrentPlayer->bond.collision_position.f[1];
            lookPos.f[2] = g_CurrentPlayer->bond.collision_position.f[2] + (g_CurrentPlayer->bond.theta_transform.f[2] * 300.0f);

            stanResetHits();

            if (stanTestLineUnobstructed(&tile, g_CurrentPlayer->bond.collision_position.f[0], g_CurrentPlayer->bond.collision_position.f[2], lookPos.f[0], lookPos.f[2], CDTYPE_CLOSEDDOORS, size.f[2], size.f[1], 0, 1.0f))
            {
                lookPos.f[1] = playerGetFloorY(tile, lookPos.f[0], lookPos.f[2]);
            }
            else
            {
                chrStanPointPointIntersection(&g_CurrentPlayer->bond.collision_position, &g_CurrentPlayer->bond.theta_transform, (coord3d *) &lookPos);
                dx = lookPos.f[0] - g_CurrentPlayer->bond.collision_position.f[0];
                dz = lookPos.f[2] - g_CurrentPlayer->bond.collision_position.f[2];
                groundDist = sqrtf((dx * dx) + (dz * dz));
                lookPos.f[1] = playerGetFloorY(tile, lookPos.f[0], lookPos.f[2]);
            }

            if (groundDist > 0)
            {
                dy = lookPos.f[1] - g_CurrentPlayer->stanHeight;

                if ((dy > -300.0f) && (dy < 500.0f))
                {
                    targetPitch = ((mathAtan2(dy, groundDist) * 360.0f) / M_TAU_F) + -4.0f;

                    if (targetPitch >= 180.0f)
                    {
                        targetPitch -= 360.0f;
                    }

                    if (targetPitch > 0)
                    {
                        targetPitch *= 0.8666667f;
                    }
                }
            }
        }

        if ((g_CurrentPlayer->movecentrerelease) && (moveData.analogWalk < 40) && (moveData.analogWalk > -40))
        {
            g_CurrentPlayer->movecentrerelease = FALSE;
        }


        if (g_PlayerIsInTank == 0)
        {
            /**
             * If the player is giving manual pitch inputs, stop the automatic look ahead pitch adjust.
             */
            if ((moveData.speedVertaDown > 0) || (moveData.speedVertaUp > 0))
            {
                g_CurrentPlayer->docentreupdown = FALSE;
                g_CurrentPlayer->prevupdown = TRUE;
                g_CurrentPlayer->automovecentre = FALSE;
            }
            else
            {
                if (moveData.disableLookAhead)
                {
                    g_CurrentPlayer->automovecentre = FALSE;
                }
                else if (g_CurrentPlayer->automovecentreenabled)
                {
                    if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < -60)))
                    {
                        g_CurrentPlayer->automovecentre = TRUE;
                    }

                    /**
                     * If the player's camera pitch is 5 degrees above the target pitch or 10 degrees below the target pitch,
                     * and move centre is allowed, look ahead (docentreupdown) can be activated.
                     */
                    if ((g_CurrentPlayer->automovecentre)
                        && (( ((targetPitch + 5.0f) < g_CurrentPlayer->vv_verta)) || (g_CurrentPlayer->vv_verta < (targetPitch + -FLOAT_TEN_A)))
                        && (g_CurrentPlayer->movecentrerelease == FALSE))
                    {
                        g_CurrentPlayer->docentreupdown = TRUE;
                    }
                }
                /**
                 * fastmovecentreenabled is never set to true, so this block can never execute. Cut option?
                 */
                else if ((g_CurrentPlayer->fastmovecentreenabled)
                    && (moveData.canLookAhead)
                    && ((moveData.analogWalk > 60) || (moveData.analogWalk < -60))
                    && (( ((targetPitch + 5.0f) < g_CurrentPlayer->vv_verta)) || (g_CurrentPlayer->vv_verta < (targetPitch + -FLOAT_TEN_A)))
                    && (g_CurrentPlayer->movecentrerelease == FALSE))
                {
                    g_CurrentPlayer->docentreupdown = TRUE;
                }

                g_CurrentPlayer->prevupdown = FALSE;
            }
        }

        /**
         * If look ahead (docentreupdown) is active, adjust the player's camera pitch to the target pitch.
         */
        if (g_CurrentPlayer->docentreupdown)
        {
            if (g_PlayerIsInTank == 0)
            {
                limit = (g_CurrentPlayer->speedverta * g_CurrentPlayer->speedverta * 0.5f) / 0.05f;

                if ((targetPitch + limit) < g_CurrentPlayer->vv_verta)
                {
                    playerUpdateSpeedVerta(1.0f);
                }
                else if (g_CurrentPlayer->vv_verta < (targetPitch - limit))
                {
                    playerUpdateSpeedVerta(-1.0f);
                }
                else
                {
                    playerUpdateSpeedVerta(0);
                }

                limit = g_CurrentPlayer->vv_verta + (2.0f * (g_CurrentPlayer->speedverta * g_GlobalTimerDelta));

                if ((targetPitch < g_CurrentPlayer->vv_verta) && (targetPitch < limit))
                {
                    g_CurrentPlayer->vv_verta = limit;
                }
                else if ((g_CurrentPlayer->vv_verta < targetPitch) && (limit < targetPitch))
                {
                    g_CurrentPlayer->vv_verta = limit;
                }
                else
                {
                    g_CurrentPlayer->vv_verta = targetPitch;
                    g_CurrentPlayer->speedverta = 0;

                    if (g_CurrentPlayer->prevupdown == FALSE)
                    {
                        g_CurrentPlayer->docentreupdown = FALSE;
                    }
                }
            }
        }
        else
        {
            if (moveData.canNaturalPitch)
            {
                fovScale = viGetFovY() / 60.0f;
                pitchInput = (f32) moveData.analogPitch / 70.0f;

                if (pitchInput > 1)
                {
                    pitchInput = 1;
                }
                else if (pitchInput < -1)
                {
                    pitchInput = -1;
                }

                if (pitchInput >= 0)
                {
                    pitchInput *= pitchInput;
                }
                else
                {
                    pitchInput *= -pitchInput;
                }

                g_CurrentPlayer->speedverta = -pitchInput * fovScale;
            }
            else if (moveData.speedVertaDown > 0)
            {
                playerUpdateSpeedVerta(moveData.speedVertaDown);

                // Bug? This is true for every value except exactly 60.
                if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < 60)))
                {
                    g_CurrentPlayer->movecentrerelease = TRUE;
                }
            }
            else if (moveData.speedVertaUp > 0)
            {
                playerUpdateSpeedVerta(-moveData.speedVertaUp);

                if ((moveData.canLookAhead) && ((moveData.analogWalk > 60) || (moveData.analogWalk < 60)))
                {
                    g_CurrentPlayer->movecentrerelease = TRUE;
                }
            }
            else
            {
                playerUpdateSpeedVerta(0);
            }

            g_CurrentPlayer->vv_verta += g_CurrentPlayer->speedverta * g_GlobalTimerDelta * 3.5f;

            if ((g_PlayerIsInTank == 1) && (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING) && (g_CurrentPlayer->vv_verta < -20.0f))
            {
                g_CurrentPlayer->vv_verta = -20.0f;
            }
        }
    }

    if (moveData.canNaturalTurn)
    {
        fovY = viGetFovY();
        turnInput = (f32) moveData.analogTurn / 70.0f;

        if (turnInput > 1)
        {
            turnInput = 1;
        }
        else if (turnInput < -1)
        {
            turnInput = -1;
        }

        if (turnInput >= 0)
        {
            turnInput *= turnInput;
        }
        else
        {
            turnInput *= -turnInput;
        }

        turnScale = (fovY / FOV_Y_F);
        g_CurrentPlayer->speedtheta = turnInput * turnScale;
    }
    else if (moveData.aimTurnLeftSpeed > 0)
    {
        playerUpdateSpeedTheta(moveData.aimTurnLeftSpeed);
    }
    else if (moveData.aimTurnRightSpeed > 0)
    {
        playerUpdateSpeedTheta(-moveData.aimTurnRightSpeed);
    }
    else
    {
        playerUpdateSpeedTheta(0);
    }

    if (g_PlayerIsInTank == 1)
    {
        if (g_EnterTankAudioState == TANK_RUN_STATE_RUNNING)
        {
            turnTarget = 0;

            if (moveData.canNaturalTurn)
            {
                turnTarget = g_CurrentPlayer->speedtheta * 0.3f;
            }
            else if (moveData.aimTurnLeftSpeed > 0)
            {
                turnTarget = playerScaleLookSpeedTheta(1) * 0.3f;
            }
            else if (moveData.aimTurnRightSpeed > 0)
            {
                turnTarget = playerScaleLookSpeedTheta(-1) * 0.3f;
            }

            for (step=0; step<g_ClockTimer; step++)
            {
                g_TankTurnSpeed = (TANKUPDATEROTATION_SCALE * g_TankTurnSpeed) + turnTarget;
            }

            g_CurrentPlayer->speedtheta = g_TankTurnSpeed * TANK_VERT_ANGLE_RAD_FACTOR;
        }
        else
        {
            g_CurrentPlayer->speedtheta = 0;
        }
    }

    if (moveData.detonating)
    {
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
        g_CurrentPlayer->hands[GUNRIGHT].weapon_current_animation = 0;
        remoteMinesDetonate();
    }

    canCycleWeapons = 1;

    if ((isMpGame()) && (frontGetMpScenario() == 2) && (invHasFlagToken()))
    {
        canCycleWeapons = 0;
    }

    if (canCycleWeapons)
    {
        if (moveData.weaponBackOffset)
        {
            gunCycleBackward();
        }

        if (moveData.weaponForwardOffset)
        {
            gunCycleForward();
        }
    }

    if (moveData.canSwivelGun)
    {
        g_CurrentPlayer->controldef = CONTROLLER_CONFIG_HONEY;
    }
    else if (moveData.canManualAim)
    {
        g_CurrentPlayer->controldef = CONTROLLER_CONFIG_KISSY;
    }

    if (g_CurrentPlayer->controldef == CONTROLLER_CONFIG_HONEY)
    {
        if (moveData.canAutoAim
            && currentPlayerGetXAutoAimEnabledRedirect()
            && g_CurrentPlayer->autoaim_target_x
            && weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_HAS_AUTO_AIM))
        {
            swivelX = g_CurrentPlayer->autoaimx;
        }
        else
        {
            swivelX = g_CurrentPlayer->speedtheta * 0.3f;
        }

        if (moveData.canAutoAim
            && currentPlayerGetYAutoAimEnabledRedirect()
            && g_CurrentPlayer->autoaim_target_y
            && weaponHasFlag(gunGetWeapon(GUNRIGHT), WEAPONSTATBITFLAG_HAS_AUTO_AIM))
        {
            swivelY = g_CurrentPlayer->autoaimy;
        }
        else
        {
            swivelY = -g_CurrentPlayer->speedverta * 0.1f;
        }

        gunUpdateCrosshairSwivel(swivelX, swivelY, MAX_AIMLOCK_SPEED_DEFAULT);
    }
    else if (g_CurrentPlayer->controldef == CONTROLLER_CONFIG_KISSY)
    {
        gunUpdateCrosshairManual(((f32) moveData.controlStickXRaw * 0.65f) / 80.0f, ((f32) moveData.controlStickYRaw * 0.65f) / 80.0f);
    }
}
