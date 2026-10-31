# Level

The level loop: loading a stage, ticking and drawing it each frame, unloading it, plus the
frame timer, the game timers and the game's one-time setup. The main loop that calls all
of this is `bossMainloop`, in `src/boss.c`.

| File | What it holds |
| --- | --- |
| `src/game/level/lv.c` | The level: loading (`lvStageLoad`), the per-frame tick (`lvTick`, `lvPlayerTick`), drawing (`lvRender`), unloading (`lvStageUnload`), the game timers, difficulty, the multiplayer time and point limits, level music, the fps counter |
| `src/game/level/frametiming.c` | The frame timer: waiting for the next frame (`frameWaitForNext`) and counting how many video frames each game frame took (`g_FrameDelta`) |
| `src/game/level/gamemode.c` | The game mode (`g_GameMode`: solo, co-op or counter-op) and its tests (`gameIsCoop`, `gameIsCounterOp`, `gameIsTwoPlayerMission` in `gamemode.h`, `gameIsMulti`) |
| `src/game/level/game.c` | The game's one-time setup (`gameInitData`: the menu state on power-on), resetting and closing the menu memory pool, the build date |

## The main loop

`bossMainloop` (in `boss.c`) runs one stage at a time. The title screen and menus count as
a stage too (`LEVELID_TITLE`). For each stage:

1. Reset the stage memory pool and the players, then `lvStageLoad`.
2. Each frame: wait for the next frame (`frameWaitForNext`), read the controllers, then
   * `lvTick`: the parts of the frame that run once, whatever the number of players;
   * for each view, `lvPlayerTick`: the player's input and movement;
   * `lvRender`: for each view, the parts that need a camera (props, firing) and the
     drawing, then the menus.
3. When a new stage is chosen, `lvStageUnload`, and back to 1.

## Loading a stage

`lvStageLoad` loads in this order: the shared textures and fonts, the menus, the level's
geometry (`bgLoad`, see [world](world.md)), the sky, the multiplayer limits and
objectives, then the models, the props and guards from the setup file
(`setupCreateProps`, see [props](props.md)), explosions and glass, and last each player
(see [player](player.md)): hands, life, intro, head bob.

## Each frame

`lvTick` sets the frame's clock (see below), runs the multiplayer game rules (time and
point limits, awards and the end of the game), applies the difficulty
(`lvApplyDifficulty`), ticks the things that move whether or not anyone sees them (room
matrices, water, sky, sparks, casings, glass, explosion particles, `propsTickOnce`), and
the level music.

`lvPlayerTick` runs for each view: `playerUpdate` (see [player](player.md)) and the
multiplayer menu. It also adds up the distance the player
walked and handles the flag and golden gun in multiplayer.

`lvRender` draws each view in a shuffled order (`playerGetShuffled`):

1. Set the viewport, clear the z-buffer, set the camera (`camSetupFrame`).
2. The sky (`skyRender`), then find the visible rooms (`bgFindVisibleRooms`).
3. Tick the props seen from this view and sort them (`propsTickPerPlayer`,
   `propsSortOnscreen`), aim, fire (`shotFireHands`), use, reload, pick up.
4. Draw the rooms and props (`bgLevelRender`), then tracers, sparks, glass, explosion
   particles.
5. The HUD (`hudRender`) and the multiplayer watch menu.

Then it draws the lines between split-screen views, and the menu if one is open.

`g_LvDrawWorld` (`lvGetDrawWorld`) turns drawing the rooms and props off while the watch
fills the screen.

## Timers

| Value | What it counts |
| --- | --- |
| `g_FrameDelta` | Video frames the last game frame took (1 at full speed, 3 at 20 fps) |
| `g_ClockTimer` | The same, but 0 while paused or while the controls are locked (`lvSetControlsLocked`): how far the game moves this frame |
| `g_GlobalTimerDelta` | `g_ClockTimer` as a float, scaled to 60 Hz on PAL (times 1.2) |
| `g_GlobalTimer` | Total game ticks this stage |
| `g_LvTickCount` | Frames the game was not paused; guards use its low bit to share work between frames |
| `g_MissionTime` | Ticks since the stage started (`lvGetMissionTimeSec`) |
| `g_SystemPowerTime` | Ticks since power on (`lvGetSystemPowerTimeSec`) |
| `g_StageTime`, `g_PowerOnTime` | The same two, but stopped while no button has been pressed for 30 seconds (`g_LvIdle`) |

The tick is 1/60 s on NTSC and 1/50 s on PAL, so code that wants seconds divides by 60 or
50. Everything in the game moves by `g_ClockTimer` or `g_GlobalTimerDelta` each frame, so
the game runs at the same speed whatever the frame rate.

## Unloading a stage

`lvStageUnload` stops the sounds, frees the language bank, guards, props, explosions,
glass, alarms, objectives and the level geometry (`bgReset`), and stores each player's
favourite weapons for the end screen.

## Traps

* `boss.c` is the main loop; `lv.c` is the level within it. "lvl" and "lv" were both
  used for the prefix; it is now `lv` everywhere.
* `g_MissionTime` and `mission_timer` (in [player](player.md), read by
  `lvGetMissionTime60`) are two different counters of the same thing.
* `game.c` is not the game: it is a few setup functions from files that used to be
  called `game_debug.c`, `initgamedata.c` and `compiletime.c`.
* `null_init_main_2`, `null_init_main_3` and `cleanup_REMOVED_` are empty, left from code
  Rare removed.
