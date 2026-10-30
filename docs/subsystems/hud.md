# HUD

Things drawn over the view or into it that aren't props or the player's guns: the
multiplayer radar, the health and armour bars, the blood that runs down the screen when
Bond dies, bullet sparks and the HUD text messages. The HUD as a whole is put together
each frame by `hudRender`, which calls into these files; ammo counters and the sight are
in [weapons](weapons.md).

| File | What it holds |
| --- | --- |
| `src/game/hud/hud.c` | `hudRender`, which draws the HUD over each player's view (hands, sight, blood, casings, watch, health bars, ammo, messages, fades) and runs the death and co-op respawn sequence; `hudRenderHealthBars`; the end-of-level credits (`hudRenderCredits`) |
| `src/game/hud/hudmsg.c` | The HUD text messages (`hudmsg*`): the bottom-left and top message queues, and the "show position" cheat's readout |
| `src/game/hud/healthbar.c` | The health and armour bars (`hudMakeDamageSegments`, `hudRenderGaugeBar`), and the green watch-screen shapes that the watch menu and the bars are built from (`watchMake*Verts`, `watchRender*`) |
| `src/game/hud/sparks.c` | Bullet sparks: short-lived camera-facing sprites where a bullet hits (`sparkCreate`), and moving sparks that fly with gravity (`movingSparkCreate`, not on PAL) |
| `src/game/hud/blood.c` | The death blood animation: decoding each frame (`bloodDecodeFrame`), smoothing it, and drawing it as a red overlay (`bloodRender`); also used by the gun barrel intro (`bloodRenderGunbarrel`) |
| `src/game/hud/radar.c` | The multiplayer radar (`radarRender`): a dot per other player |

## hudRender

`hudRender` runs once per player view, after the world is drawn. It does more than draw:
it also fires the guns for the frame (`gunUpdateAndFireBothHands`), runs counter-op's
guard takeover, and runs death. In order it does:

1. The guns' firing, then bullet casings, the gun hands and the watch.
2. The health bars, when showing; the objective status messages (solo).
3. Death: the blood runs down, the screen fades, then the level restarts (solo), the player
   respawns at a pad when a button is pressed (multiplayer), or a co-op player can respawn
   next to the partner once `COOP_RESPAWN_DELAY60` has passed (`coopRespawnPlayer`).
4. The HUD messages, the sight, ammo, the countdown timer, the radar and the screen fade.

During the intro camera only the messages, the timer, the fade and the credits are drawn.

## HUD messages

The bottom messages are pickups, objective updates and level text. In solo they queue, up
to 5, and each shows for `HUDMSG_BOTTOM_TIME` (shorter when more are waiting); in
multiplayer each player has one, replaced by the next. `bondmesscnt` in the player times
the one showing (`hudmsgTickIntroText`). `hudmsgsSetOff` hides them, for example while a
cutscene is playing.

The top messages are characters speaking to Bond: a queue of 2, timed by
`hudmsgTopTick`. In co-op the top text is shared, so each message remembers the player it
was for and only shows in that player's view.

On US, `hudmsgSetFonts` picks the font for all bottom messages; PAL passes a font with
each message.

## Health and armour bars

The bars are two curved strips, health on one side and armour mirrored on the other,
shown for a moment when Bond is hit or picks up armour. `hudMakeDamageSegments` builds a
strip's vertices for a value from 0 to 1: an arc of 23 segments, bright up to the current
value and faint after it, in warm colours for health and blue for armour (the sign of
`isArmour` picks the side and colour). `hudRenderGaugeBar` turns the vertices into a
display list. The watch's health page draws the same bars.

## Bullet sparks

`sparkCreate` adds a spark at a world position, in one of the colours in `g_SparkColors`;
guns, guards and objects call it on impact. There are 20 fixed sparks (`g_Sparks`) and 50
moving ones (`g_MovingSparks`), which `lvTick` ticks (`sparksTickAll`) and `lvRender` draws
for each view after the rooms (`sparksRenderAll`).

## The death blood

When Bond dies, blood runs down over the view. The animation is stored as one block of
compressed data (`g_BloodAnimData`), decoded one frame at a time: `bloodNextFrame` decodes
the next frame into an 80 by 96 greyscale image, transposes it, smooths it twice and packs
it to 4 bits, and `bloodRender` stretches it over the view as a red, semi-transparent
texture. `bloodNextFrame` returns true once the last frame is decoded. The intro's gun barrel sequence uses
the same animation for the blood that runs down after Bond shoots.

`screenClearBlack` (fill the frame buffer with black) and `gunbarrelSetupDraw` live in
`blood.c` too but are used by the menus and the title sequence.

## Radar

`radarRender` draws the radar in the top right of each player's view in multiplayer, unless
the "no radar" cheat is on: a background image, the player's own dot in the middle, then a
dot for each other player, placed by their direction and distance. In team, flag and
golden gun games the player's own dot is coloured, blue while holding the flag or golden
gun.

## Traps

* The file names were misleading before this mapping: `glass2.c` (now `healthbar.c` and
  `sparks.c`) had nothing to do with glass, and `bondwalk2.c` (now `gfx/texrect.c`) only
  draws a textured rectangle.
* On PAL, there are no moving sparks: the `movingSparks*` functions just call the plain
  spark ones, with the wrong arguments in places. The rewrite should use one version.
* The spark structure fields are still mostly `unkXX`.
* `sparkGetColor` and `sparkSetColor` are never called.
