/**
 * The ammo HUD and the gun sight: the loaded/held counters and icons
 * in the corner of the screen, and the crosshair.
 */
#include "assets/obseg/text/LgunE.h"
#include "fr.h"
#include "game/gfx/imagebank.h"
#include "game/gfx/texcmds.h"
#include "game/gfx/texrect.h"
#include "game/menu/watchmenu.h"
#include "game/player/bondview.h"
#include "game/player/gun.h"
#include "game/player/player.h"
#include "game/text/text.h"
#include "game/hud/ammohud.h"

#if VERSION_EU
// Corrects the sight's shape for the PAL screen
static const f32 g_GunSightAspectRatio = 25.0f / 21.0f;
#endif


/**
 * Hides or shows the ammo HUD for one reason (a bit in gunammooff). It is
 * only shown while no reason hides it.
 */
void gunSetGunAmmoVisible(s32 reason, bool enable) {

	if (enable)
    {
		g_CurrentPlayer->gunammooff &= ~reason;
		return;
	}

	g_CurrentPlayer->gunammooff |= reason;
}


/**
 * Draws one HUD icon from the image bank at x, y, optionally flipped and
 * translucent, in the given colour.
 */
Gfx *hudDrawImage(Gfx *gdl, struct sImageTableEntry *tconfig, f32 x, f32 y, f32 baseY, s32 flipX, f32 offsetY, s32 translucent, s32 red, s32 green, s32 blue, s32 alpha)
{
    f32 xy[2];
    f32 halfed[2];
 
    gDPSetColorDither(gdl++, G_CD_DISABLE);
    gDPSetTexturePersp(gdl++, G_TP_NONE);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_TILE);
    gDPSetTextureFilter(gdl++, G_TF_POINT);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    xy[0] = ((u32)tconfig->width * 0.5f) - (f32)(tconfig->width / 2);

    if (flipX != 0) 
    { 
        xy[0] = -xy[0]; 
    }

    xy[0] = xy[0] + x;
 
    if (0.0f <= y)
    {
        xy[1] = y - (((f32) ((u32) tconfig->height)) * 0.5f);
    }
    else
    {
        xy[1] = -((((f32)((u32)tconfig->height)) * 0.5f) - ((f32)(tconfig->height / 2)));
        xy[1] = baseY + xy[1];
        xy[1] = xy[1] + offsetY;
    }
 
    halfed[0] = ((f32) ((u32) tconfig->width)) * 0.5f;
    halfed[1] = ((f32) ((u32) tconfig->height)) * 0.5f;
 
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_1CYCLE);
    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, 0, 0, 0, 0);
    gDPFillRectangle(gdl++, VI_SX(((s32)(xy[0] - halfed[0])) - 1), ((s32)(xy[1] - halfed[1])) - 1, VI_SX(((s32)(xy[0] + halfed[0])) + 1), ((s32)(xy[1] + halfed[1])) + 1);
 
    texSelect(&gdl, tconfig, translucent ? TEXBLEND_TRANSLUCENT : TEXBLEND_OPAQUE, TEXZBUF_OFF, 0);
    texRectDrawImage(&gdl, xy, halfed, tconfig->width, tconfig->height, 0, 0, 1, red, green, blue, alpha, 0 < tconfig->level, 0);
 
    gDPPipeSync(gdl++);
    gDPSetColorDither(gdl++, G_CD_BAYER);
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    gDPSetAlphaCompare(gdl++, G_AC_NONE);
    gDPSetTextureLOD(gdl++, G_TL_LOD);
    gDPSetTextureFilter(gdl++, G_TF_BILERP);
    gDPSetTextureConvert(gdl++, G_TC_FILT);
    gDPSetTextureLUT(gdl++, G_TT_NONE);
 
    return gdl;
}


/**
 * hudDrawImage in white at full opacity.
 */
Gfx *hudDrawImageDefault(Gfx *gdl, sImageTableEntry *tconfig, f32 x, f32 y, f32 baseY, s32 flipX, f32 offsetY, s32 translucent)
{
    return hudDrawImage(gdl, tconfig, x, y, baseY, flipX, offsetY, translucent, 0xff, 0xff, 0xff, 0xff);
}


/**
 * Draws a HUD string at x, y, aligned by halign/valign, with an optional outline.
 */
Gfx *gunDrawHudString(Gfx *gdl, char *text, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    s32 x1;
    s32 y1;
    s32 x2;
    s32 y2;
    s32 textheight;
    s32 textwidth;

    x1 = 0;
    y1 = 0;
    x2 = 0;
    y2 = 0;
    textwidth = 0;
    textheight = 0;

    textMeasure(&textheight, &textwidth, text, g_FontBankGothicChars, g_FontBankGothic, 0);

    if (halign == HUDHALIGN_LEFT) { // left
		x2 = x + textwidth;
		x1 = x;
	} else if (halign == HUDHALIGN_RIGHT) { // right
		x1 = x - textwidth;
		x2 = x;
	} else if (halign == HUDHALIGN_MIDDLE) { // middle
		x2 = x + textwidth / 2;
		x1 = x2 - textwidth;
	}

    if (valign == HUDVALIGN_TOP) { // top
		y2 = y + textheight;
		y1 = y;
	} else if (valign == HUDVALIGN_BOTTOM) { // bottom
		y1 = y - textheight;
		y2 = y;
	} else if (valign == HUDVALIGN_MIDDLE) { // middle
		y2 = y + textheight / 2;
		y1 = y2 - textheight;
	}

    gdl = textRenderBlackBox(gdl, &x1, &y1, &x2, &y2);

    if (outline) {
        gdl = textRenderOutlined(gdl, &x1, &y1, text, g_FontBankGothicChars, g_FontBankGothic, -1, 0x646464FF, (s32) viGetX(), viGetY(), 0, 0);
    } else {
        gdl = textRender(gdl, &x1, &y1, text, g_FontBankGothicChars, g_FontBankGothic, 0xFF00B0, (s32) viGetX(), viGetY(), 0, 0);
    }

    return gdl;
}


/**
 * gunDrawHudString for a number.
 */
Gfx *gunDrawHudInteger(Gfx *gdl, s32 value, s32 x, s32 halign, s32 y, s32 valign, bool outline)
{
    char buffer[12];
    sprintf(buffer, "%d\n", value);
    return gunDrawHudString(gdl, buffer, x, halign, y, valign, outline);
}


/**
 * Draw magazine ammo number, ammo type icon, and total ammo number at the bottom right of the viewport.
 * Render an additional ammo counter at the bottom left of the viewport when dual wielding.
 */
Gfx *hudRenderAmmo(Gfx *gdl)
{
    ITEM_IDS weapon_left;
    ITEM_IDS weapon_right;
    s32 ammotype;
    s32 leftx;
    s32 rightx;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset_r;
    s32 textwidth_r;
    u32 imageoffset_l;
    s32 textwidth_l;

    if (g_CurrentPlayer->gunammooff == 0)
    {
        if (g_CurrentPlayer->mpmenuon == 0)
        {
            weapon_left = gunGetWeapon(GUNLEFT);
            weapon_right = gunGetWeapon(GUNRIGHT);

            if (!viewIsHalfWidth())
            {
                leftx = 59;
                rightx = 59;
            }
            else if (coopPlayerPerson(playerGetCurrentNum()) & 1)
            {
                leftx = 43;
                rightx = 127;
            }
            else
            {
                leftx = 59;
                rightx = 109;
            }

            if (weapon_right != ITEM_UNARMED)
            {
                ammotype = weaponGetAmmoType(weapon_right);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[0].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_r = g_AmmoStats[ammotype].IconImage;
                    textwidth_r = 5;

                    if (imageoffset_r != 0)
                    {
                        imageoffset_r += g_ImageBankOffset;
                        gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset_r, (camGetScreenLeft() + camGetScreenWidth()) - (f32)rightx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 0,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 0,
#endif
                            g_AmmoStats[ammotype].IconYOffset, 1);
                        textwidth_r = ((u8 *)imageoffset_r)[4];
                    }

                    gdl = textSetupDraw(gdl);

                    if (weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) - (textwidth_r / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || weaponHasFlag(weapon_right, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, (((viGetViewLeft() + viGetViewWidth()) - rightx) + ((textwidth_r + 1) / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = textEndDraw(gdl);
                }
            }

            if (weapon_left != ITEM_UNARMED)
            {
                ammotype = weaponGetAmmoType(weapon_left);

                if (ammotype != 0
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
                    && g_CurrentPlayer->hands[1].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
                    && !weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
                {
                    imageoffset_l = g_AmmoStats[ammotype].IconImage;
                    textwidth_l = 5;

                    if (imageoffset_l != 0)
                    {
                        imageoffset_l += g_ImageBankOffset;
                        gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset_l, camGetScreenLeft() + (f32)leftx, -1.0f,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 30, 1,
#else
                            (viGetViewTop() + viGetViewHeight()) - 20, 1,
#endif
                            g_AmmoStats[ammotype].IconYOffset, 1);
                        textwidth_l = ((u8 *)imageoffset_l)[4];
                    }

                    gdl = textSetupDraw(gdl);

                    if (weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        magammo = 0;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        if (weapon_left == weapon_right)
                        {
                            reserveammo += g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                        }
                    }
                    else
                    {
                        magammo = g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                        reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
                    }

                    if (!weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, magammo, ((viGetViewLeft() + leftx) + (textwidth_l / 2)) + 3, 1,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    if (reserveammo > 0 || weaponHasFlag(weapon_left, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
                    {
                        gdl = gunDrawHudInteger(gdl, reserveammo, ((viGetViewLeft() + leftx) - ((textwidth_l + 1) / 2)) - 4, 0,
#if defined(VERSION_EU)
                            (viGetViewTop() + viGetViewHeight()) - 28, 2, 1);
#else
                            (viGetViewTop() + viGetViewHeight()) - 18, 2, 1);
#endif
                    }

                    gdl = textEndDraw(gdl);
                }
            }
        }
    }

    return gdl;
}


/**
 * Draws the ammo HUD: the loaded and held counters for each hand and the
 * ammo type icon.
 */
Gfx *gunDrawWatchAmmoDisplay(Gfx *gdl)
{
    ITEM_IDS offhanditem;
    ITEM_IDS item;
    s32 ammotype;
    s32 reserveammo;
    s32 magammo;
    u32 imageoffset;
    s32 textwidth;

    offhanditem = gunGetWeapon(1);
    item = gunGetWeapon(0);

    if (item != ITEM_UNARMED)
    {
        ammotype = weaponGetAmmoType(item);

        if (ammotype != 0
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
            && g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
            && !weaponHasFlag(item, WEAPONSTATBITFLAG_HIDE_AMMO_DISPLAY))
        {
            imageoffset = g_AmmoStats[ammotype].IconImage;
            textwidth = 5;

            weaponGetStats(item);

            if (imageoffset != 0)
            {
                imageoffset += g_ImageBankOffset;

                // Draw the ammo icon
#if defined(VERSION_EU)
                gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset, 200.0f, 208.0f, (viGetViewTop() + viGetViewHeight()) - 30, 0, g_AmmoStats[ammotype].IconYOffset, 1);
#else
                gdl = hudDrawImageDefault(gdl, (sImageTableEntry *)imageoffset, 200.0f, 180.0f, (viGetViewTop() + viGetViewHeight()) - 20, 0, g_AmmoStats[ammotype].IconYOffset, 1);
#endif

                textwidth = ((u8 *)imageoffset)[4];
            }

            gdl = textSetupDraw(gdl);

            if (weaponHasFlag(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                magammo = 0;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype] + g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;

                if (offhanditem == item)
                {
                    reserveammo += g_CurrentPlayer->hands[1].weapon_ammo_in_magazine;
                }
            }
            else
            {
                magammo = g_CurrentPlayer->hands[0].weapon_ammo_in_magazine;
                reserveammo = g_CurrentPlayer->ammoheldarr[ammotype];
            }

            if (!weaponHasFlag(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the magazine ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, magammo, 196 - (textwidth / 2), 0, 177, 2, 0);
#endif
            }

            if (reserveammo > 0 || weaponHasFlag(item, WEAPONSTATBITFLAG_NO_CLIP_RELOADS))
            {
                // Draw the reserve ammo count.
#if defined(VERSION_EU)
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 205, 2, 0);
#else
                gdl = gunDrawHudInteger(gdl, reserveammo, 203 + ((textwidth + 1) / 2), 1, 177, 2, 0);
#endif
            }

            gdl = textEndDraw(gdl);
        }
    }

    return gdl;
}


/**
 * Hides or shows the sight for one reason (a bit in gunsightmode).
 */
void gunSetSightVisible(s32 reason, bool visible)
{
    if (visible)
    {
        g_CurrentPlayer->gunsightmode &= ~reason;
        return;
    }

    g_CurrentPlayer->gunsightmode |= reason;
}


/**
 * Draws the current player's crosshair or zoomed sight.
 */
void gunDrawSight(Gfx **gdl) {

    Gfx *dl;
    f32 xypos[2];
    f32 halfedxy[2];

    if ((g_CurrentPlayer->gunsightmode == 0) && (g_CurrentPlayer->mpmenuon == FALSE)) {
        dl = *gdl;
        texSelect(&dl, g_ImageCrosshair, TEXBLEND_CLOUD, TEXZBUF_OFF, 0);

        xypos[0] = g_CurrentPlayer->crosshair_angle.f[0];
        xypos[1] = g_CurrentPlayer->crosshair_angle.f[1];
        halfedxy[0] = 16.0f;
        halfedxy[1] = 16.0f;

        if (optionsGetScreenRatio() == SCREEN_RATIO_16_9) {
            halfedxy[0] = halfedxy[0] * 0.75f;
        }
#ifdef VERSION_EU
        halfedxy[1] = halfedxy[1] * g_GunSightAspectRatio;
#endif
        texRectDrawImage(&dl, xypos, halfedxy, 0x20, 0x20, 0, 0, 1, 0xFF, 0xFF, 0xFF, 0x6E, (g_ImageCrosshair->level > 0), 0);
        *gdl = dl;
    }
}
