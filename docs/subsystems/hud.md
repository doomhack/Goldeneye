# HUD

Things drawn over the view or into it that aren't props or the player's guns: the
multiplayer radar, the health and armour bars, the blood that runs down the screen when
Bond dies, and bullet sparks. The HUD as a whole is put together by `hudRender` (see
[player](player.md)), which calls into these files; ammo counters and the sight are in
[weapons](weapons.md), and HUD text messages are `hudmsg*` in [player](player.md).

| File | What it holds |
| --- | --- |
| `src/game/hud/healthbar.c` | The health and armour bars (`hudMakeDamageSegments`, `hudRenderGaugeBar`), and the green watch-screen shapes that the watch menu and the bars are built from (`watchMake*Verts`, `watchRender*`) |
| `src/game/hud/sparks.c` | Bullet sparks: short-lived camera-facing sprites where a bullet hits (`sparkCreate`), and moving sparks that fly with gravity (`movingSparkCreate`, not on PAL) |
| `src/game/hud/blood.c` | The death blood animation: decoding each frame (`bloodDecodeFrame`), smoothing it, and drawing it as a red overlay (`bloodRender`); also used by the gun barrel intro (`bloodRenderGunbarrel`) |
| `src/game/hud/radar.c` | The multiplayer radar (`radarRender`): a dot per other player |

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
