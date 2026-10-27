/**
 * The health and armour bars: hudMakeDamageSegments builds the vertices of one curved bar
 * for a value from 0 to 1, and hudRenderGaugeBar draws them. Also the helpers that build
 * and draw the green shapes of the watch screen (watchMake*Verts, watchRender*), which the
 * watch menu in options.c uses.
 */

#include <ultra64.h>
#include <limits.h>
#include "game/world/bg.h"
#include "game/world/roommtx.h"
#include "game/player/bondview.h"
#include "game/gfx/dyn.h"
#include "game/math/trig.h"
#include "gbi_extension.h"
#include "game/prop/glass.h"
#include "game/gfx/imagebank.h"
#include "game/level/lv.h"
#include "game/mission/objective.h"
#include "random.h"
#include "game/math/physics.h"
#include "game/hud/healthbar.h"

#define GAUGE_BAR_VERTEX_PAIR_STRIDE (2 * sizeof(struct WatchVertex))

#if defined(LEFTOVERDEBUG)
/*
  Render Health Bars
  AI Comment: This function populates a radial array of HUD elements (HealthSegments) with position and color data
  based on a damage value (HealthValue) and a display mode (isArmour). It loops through 23 segments, calculating their
  screen-space coordinates and visual properties using trigonometric functions.
  @healthSegments : A pointer to an Array of 46 vertices.
  @isArmour : Armour/Health if positive/negative
  @numsegments : Not Used
  @HealthValue : amount of health/armour 0-10
  @Address: 7F0A2F30
*/
void hudMakeDamageSegments(struct damage_display_val *HealthSegments, s32 numSegments, s32 isArmour, f32 HealthValue)
{
	s32 unused;
    s32 i;
    s32 pairIndex;
    f32 angleRadians;

    HealthValue *= 8;


    //for 145.2 to 35.2 degrees, calculate health/armour
    for (i=0; i<23; i++)
    {
        //This line calculates an angle in radians, starting from 142° (cast truncated) and decreasing by 5° per iteration.
        angleRadians = ((f32) (s32)(142.5 - (i*5))* M_PI_F * 2) / 360;

        for (pairIndex = 0; pairIndex < 2; pairIndex++)
        {
            s16 radialOffsetX = (((sinf(angleRadians) * 4 * 130 * (6 - pairIndex)) / 5) * isArmour);
            s16 radialOffsetZ = (((cosf(angleRadians) * 4) * 130 * (6 - pairIndex)) / 5);

            HealthSegments->pos.x    = (radialOffsetX + 1);
            HealthSegments->pos.y    = 0;
            HealthSegments->pos.z    = -radialOffsetZ;
            HealthSegments->normal.x = 0;
            HealthSegments->normal.y = 0;
            HealthSegments->normal.z = 0;
            HealthSegments->colour.r = 255;
            HealthSegments->colour.g = 255;
            HealthSegments->colour.b = 255;

            if (isArmour >= TRUE) //armour shade
            {
                HealthSegments->colour.r = (int)(96 - (cosf(angleRadians) * 96));
                HealthSegments->colour.g = (int)(127 - (cosf(angleRadians) * 127));
                HealthSegments->colour.b = 255;
            }
            else if (isArmour < FALSE) //health shade
            {
                HealthSegments->colour.g =  (int)(127 - (cosf(angleRadians) * 127));
                HealthSegments->colour.b = (int)(32 - (cosf(angleRadians) * 32));
            }

            // segments 0-9 are single, 10-22 are doubled with single gaps
            // IF i < damage fill, if fractional, shade else no fill.
            if (i < 10) //145 to 95
            {
                //full shade
                if (i <= (((int)HealthValue * 2) - 1))
                {
                    HealthSegments->colour.a = 255;
                }
                // Fraction fill
                else if ((i < (int)(HealthValue * 2.0f)) && (i > (((int)HealthValue * 2) - 1))) //yes, it looks like one is float, the other is cast
                {
                    HealthSegments->colour.a = (int)((HealthValue - (int)HealthValue) * 207) + 48;
                }
                else //no fill
                {
                    HealthSegments->colour.a = 48;
                }
            }
            else if (i >= 10) //95 to 35
            {
                if ( i <= (9 + ((HealthValue - 5.0f) * 4)))
                {
                    HealthSegments->colour.a = 255;
                }
                else if (i <= (((int)(((HealthValue - 5.0f) * 4) + 0.5f) + 9)) && (i > (((int)(HealthValue - 5.0f) * 2) + 8)))
                {
                    HealthSegments->colour.a = (int)((HealthValue - (int)HealthValue) * 207) + 48;
                }
                else
                {
                    HealthSegments->colour.a = 48;
                }
            }
            HealthSegments++;
        }
    }
}
#endif

#if !defined(LEFTOVERDEBUG)
void hudMakeDamageSegments(struct damage_display_val *HealthSegments, s32 numSegments, s32 isArmour, f32 HealthValue)
{
	s32 new_var2;
	f32 new_var;
	s32 pairIndex;
	s32 i;
	s16 temp_s1;
	s32 sp80;
	f32 temp_f18;
	f32 temp_f4;
	f32 sp74;
	f32 angleRadians;
	HealthValue *= 8.0f;
	sp80 = 0;

	for (i = 0; i < 23; i++)
	{
		temp_f18 = (f32) ((s32) (142.5 - ((f64) sp80)));
		angleRadians = ((temp_f18 * M_PI_F) * ((f32) 2)) / 360.0f;
		for (pairIndex = 0; pairIndex < 2; pairIndex++)
		{
	        sp74 = (f32) isArmour;
			temp_s1 = (s16) ((s32) (((((sinf(angleRadians) * 4.0f) * 130.0f) * ((f32) (6 - pairIndex))) / 5.0f) * sp74));
			temp_f4 = cosf(angleRadians) * 4.0f;
			HealthSegments->pos.x = temp_s1 + 1;
			HealthSegments->pos.y = 0;
			HealthSegments->normal.x = 0;
			HealthSegments->normal.y = 0;
			new_var = HealthValue - 5.0f;
			HealthSegments->normal.z = 0;
			HealthSegments->colour.r = 0xFF;
			HealthSegments->colour.g = 0xFF;
			temp_s1 = ((temp_f4 * 130.0f) * ((f32) (6 - pairIndex))) / 5;
			HealthSegments->colour.b = 0xFF;
			HealthSegments->pos.z = (s16) (-((s32) temp_s1));
			if (isArmour > 0)
			{
				HealthSegments->colour.r = (s8) ((s32) (96.0f - (cosf(angleRadians) * 96.0f)));
				HealthSegments->colour.g = (s8) ((s32) (127.0f - (cosf(angleRadians) * 127.0f)));
				HealthSegments->colour.b = 0xFF;
			}
			else if (isArmour < 0)
			{
				HealthSegments->colour.g = (s8) ((s32) (127.0f - (cosf(angleRadians) * 127.0f)));
				HealthSegments->colour.b = (s8) ((s32) (32.0f - (cosf(angleRadians) * 32.0f)));
			}
			if (i < 10)
			{
				if (((((s32) HealthValue) * 2) - 1) >= i)
				{
					HealthSegments->colour.a = 0xFF;
				}
				else if ((i < ((s32) (2.0f * HealthValue))) && (((((s32) HealthValue) * 2) - 1) < i))
				{
					HealthSegments->colour.a = (s8) (((s32) ((HealthValue - ((f32) ((s32) HealthValue))) * 207.0f)) + 0x30);
				}
				else
				{
					HealthSegments->colour.a = 0x30;
				}
			}
			else if (i >= 10)
			{
				if (((f32) i) <= (9.0f + ((HealthValue - 5.0f) * 4.0f)))
				{
					HealthSegments->colour.a = 0xFF;
				}
				else
				{
					new_var2 = i;
					if (((((s32) ((new_var * 4.0f) + 0.5f)) + 9) >= new_var2) && (((((s32) (HealthValue - 5.0f)) * 2) + 8) < new_var2))
					{
						HealthSegments->colour.a = (s8) (((s32) ((HealthValue - ((f32) ((s32) HealthValue))) * 207.0f)) + 0x30);
					}
					else
					{
						HealthSegments->colour.a = 0x30;
					}
				}
			}
			HealthSegments += 1;
		}
		sp80 += 5;
	}
}
#endif


/**
 * Address: 7F0A3330
 *
 * Creates the display list for HUD and watch health and armor bars.
 */
Gfx *hudRenderGaugeBar(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices)
{
    s8 i;

    for (i = 0; i <= (numvertices / 2 - 2); i++) 
    {
        gSPVertex(gdl++, vtxaddr, 4, 0);

        if (i >= 9) 
        {
            if ((i + 3) % 4) 
            {
                gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            }
        } 
        else if (i < 9) 
        {
            if ((i & 1) == 0) 
            {
                gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            }
        }

        vtxaddr += GAUGE_BAR_VERTEX_PAIR_STRIDE;
    }

    gSPEndDisplayList(gdl++);

    return gdl;
}


/**
 * Address: 7F0A33F8
 */
void watchMakeArcVerts(struct WatchVertex *vtx, s32 numverts, f32 scale, s32 arg3)
{
    f32 angle;
    s32 i;
    s16 sinval;
    s16 cosval;

    if (arg3)
    {
        vtx->coord1.x = 1;
        vtx->coord1.y = 0;
        vtx->coord1.z = 0;
        vtx->coord2.x = 0;
        vtx->coord2.y = 0;
        vtx->coord2.z = 0;
        vtx->color.r = 0;
        vtx->color.g = 0x2c;
        vtx->color.b = 0;
        vtx->color.a = 0xb0;

        vtx++;
    }

    for (i = 7; i <= (numverts - 7); i += 2)
    {
        angle = ((f32)i * M_PI_F) / numverts;
        sinval = sinf(angle) * 520.0f * scale;
        cosval = cosf(angle) * 520.0f * scale;

        vtx->coord1.x = 1 + sinval;
        vtx->coord1.y = 0;
        vtx->coord1.z = -cosval;
        vtx->coord2.x = 0;
        vtx->coord2.y = 0;
        vtx->coord2.z = 0;
        vtx->color.r = 0 - (cosf(angle) * 0);
        vtx->color.g = 44.0f - (cosf(angle) * 20.0f);
        vtx->color.b = 0 - (cosf(angle) * 0);
        vtx->color.a = 0xb0;

        vtx++;

        if ((i != 0) && (i < numverts))
        {
            vtx->coord1.x = 1 + -sinval;
            vtx->coord1.y = 0;
            vtx->coord1.z = -cosval;
            vtx->coord2.x = 0;
            vtx->coord2.y = 0;
            vtx->coord2.z = 0;

            vtx->color.r = 0xFF;
            vtx->color.g = 0xFF;
            vtx->color.b = 0xFF;

            vtx->color.r = 0 - (cosf(angle) * 0);
            vtx->color.g = 44.0f - (cosf(angle) * 20.0f);
            vtx->color.b = 0 - (cosf(angle) * 0);
            vtx->color.a = 0xb0;

            vtx++;
        }
    }
}


/**
 * Address: 7F0A3978
 */
Gfx *watchRenderBackground(Gfx *gdl, struct WatchVertex *watch_verts, s32 unused_arg2, s32 drawFan)
{
    s8 i;
    struct WatchVertex *orig;

    if (drawFan) 
    {
        struct WatchVertex *vtx;

        orig = watch_verts;
        watch_verts++;
        vtx = watch_verts;

        i = 7;

        gSPVertex(gdl++, &vtx[14], 4, 0);
        gSPVertex(gdl++, orig, 1, 4);
        gSP2Triangles(gdl++, 2, 4, 3, 0, 0, 0, 0, 0);

        for (; i >= 0; i--) {
            gSPVertex(gdl++, &vtx[2 * i], 4, 0);
            gSPVertex(gdl++, orig, 1, 4);
            gSP2Triangles(gdl++, 0, 4, 2, 0, 1, 3, 4, 0);
        }

        gSP2Triangles(gdl++, 0, 1, 4, 0, 0, 0, 0, 0);
    } 
    else 
    {
        for (i = 0; i < 8; i++)
        {
            gSPVertex(gdl++, watch_verts, 4, 0);
            gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            watch_verts += 2;
        }
    }

    gSPEndDisplayList(gdl++);

    return gdl;
}


/**
 * Setup watch rectangles in the usual manner.
 * This is called to setup the screen select rectangles, but note
 * that the colors are overwritten in watchMenuSetTabColours.
 * Also used to initialize watch static.
 * @param vtx: Pointer to first vertex in a {@code struct WatchRectangle}.
 * @param startx:
 * @param startz:
 * @param width:
 * @param height:
 * @param horizontal_offset:
 * @param vertical_offset:
*/
struct WatchVertex *watchMakeRectVerts(struct WatchVertex *vtx, s32 startx, s32 startz, s32 width, s32 height, s32 horizontal_offset, s32 vertical_offset)
{
    s32 i;
    s32 j;
    s32 xval;
    s32 zval;

    i = 0;
    j = 0;
    xval = startx + horizontal_offset;

    if (i);
    if (j);
    if (vtx);
    if (width);
    if (height);

    for (i = 0; i < 2; i++, xval += width)
    {
        startz ^= 0;
    
        if (vertical_offset);
        if (height);

        for (j = 0, zval = startz + vertical_offset; j < 2; j++, zval += height)
        {
            vtx->coord1.AsArray[0] = xval;
            vtx->coord1.AsArray[1] = 0;
            vtx->coord1.AsArray[2] = zval;

            vtx->coord2.AsArray[0] = 0;
            vtx->coord2.AsArray[1] = 0;
            vtx->coord2.AsArray[2] = 0;

            vtx->color.rgba[0] = 0x20;
            vtx->color.rgba[1] = 0x70;
            vtx->color.rgba[2] = 0x20;
            vtx->color.rgba[3] = 0xF0;

            vtx++;
        }
    }

    return vtx;
}


Gfx *watchRenderQuad(Gfx *gdl, s32 *arg1)
{
    gSPVertex(gdl++, arg1, 4, 0);

    // gfxdis can't parse this, but maybe?: gSPModifyVertex(gdl++, 16, 0, 0x2110);
    // manual specification:
    {								\
        Gfx *_g = (Gfx *)(gdl++);		\
        _g->words.w0 = 0xB1000032;	\
        _g->words.w1 = 0x2110;		\
    }

    return gdl;
}
