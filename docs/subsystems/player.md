# Player

Bond, or each player in multiplayer and co-op: movement and collision, the camera, the
watch and pause tilt, health and death, and the tank. The HUD and its messages are in
[hud](hud.md). Guns, firing and the
inventory are in [weapons](weapons.md).

| File | What it holds |
| --- | --- |
| `src/game/player/player.c` | The player list: `g_playerPointers`, the current player (`playerSetCurrent`), allocating each player (`playerInit`), the draw order, per-player screen size and field of view, the hands' first setup (`playerInitHands`), and small getters (`playerGetPos`, `playerGetEyeHeight` and the like) |
| `src/game/player/playerlife.c` | Choosing a spawn pad, resetting a player for a new life (`playerResetLife`, `playerStartNewLife`), crouch and lean, and the level-wide flags AI scripts set |
| `src/game/player/playerupdate.c` | The per-frame update (`playerUpdate`, `playerMove`, `playerMoveFrozen`), height and falling (`playerUpdateVertical`), getting in and driving the tank, the mission timer |
| `src/game/player/playerbody.c` | Bond's body: loading the body and head models and the guns in its hands, cuffs, the player prop tick (`playerTick`) that poses it, and drawing it |
| `src/game/player/playerinput.c` | Input (`playerProcessInput`), auto-aim, look and walk speeds |
| `src/game/player/playerdamage.c` | Health, armour, damage (`playerTakeDamage`), the damage flash and health bars, dying (`playerDie`), explosions round the player |
| `src/game/player/playerfade.c` | The screen fade over a player's view and fading the player's body |
| `src/game/player/playertune.h` | Movement, aiming and timing constants, with their PAL values |
| `src/game/player/cammode.c` | Camera modes: switching (`playerSetCameraMode`, `playerAdvanceCameraMode`), the intro, swirl, cutscene, death and orbit cameras (`playerTickFrozenCamera`), co-op's full-screen view (`coopUpdateSoloView`) |
| `src/game/player/camview.c` | The camera helpers (`cam*`): screen and view transforms, the view matrices, the frustum planes, the viewports, the frame's camera setup (`camSetupFrame`) |
| `src/game/player/watch.c` | The pause watch: raising and lowering the arm (`watchTick`, `watchStepAnim`), the pause tilt and zoom, opening and closing it (`watchTrigger`), drawing the arm and watch (`watchRender`) |
| `src/game/player/respawn.c` | Respawning: multiplayer at a pad (`mpRespawnPlayer`), co-op next to the partner (`coopRespawnPlayer`) |
| `src/game/player/counterop.c` | Counter-op: player 2 taking over guards, the suicide pill, the end screen's stats |
| `src/game/player/playermove.c` | Moving with collision (`playerMoveWithCollision`, `playerTestMove`), the player's collision state (`playerResetCollision`), keeping the player prop in its rooms (`playerUpdateRooms`) |
| `src/game/player/tank.c` | The tank: its collision box (`tankTestCollision`, `tankTryMoveTo`), the turret (`tankUpdateTurretOffset`, `tankGetTurretDir`), co-op ownership (`coopTankBeginPlayer`) |
| `src/game/player/playerintro.c` | Level start for a player: the intro section of the setup file (`playerApplyIntro`), co-op spawn points, loading projectile models |
| `src/game/player/coop.c` | Co-op and counter-op state: whose controller and settings each player uses (`coopPlayerPerson`), reviving (`coopPartnerCanRevive`), the last player to use an object, the split-screen layout |
| `src/game/player/headbob.c` | Head bob: the hidden walk animation whose root motion moves the camera up and down as Bond walks, and breathing |

## Players

A `struct player` (in `player.h`) holds everything about one player, about 11 KB of it.
`playerInit` allocates one per player at level load. `g_CurrentPlayer` points at the one
being worked on, and almost every function in this subsystem works on it; the level loop
and the prop loop call `playerSetCurrent` before each player's turn. The functions named
`currentPlayer*` and most `player*` functions act on the current player. `g_playerPerm`
is the current player's data that lasts between levels (handicap, stats).

Each player has a view: its own rectangle of the screen, field of view and camera.
`getViewCount` is the number drawn this frame, which in co-op can be 1 while one player's
view fills the screen. Views are drawn in a shuffled order (`playersShuffleOrder`,
`playerGetShuffled`), so no player is always drawn first.

## Level start

For each player, `lvStageLoad` (in `lv.c`) calls:

1. `playerInitHands`: gun buffers and empty hands.
2. `playerResetLife`: position, health, armour, speeds and options back to defaults.
3. `playerApplyIntro`: reads the intro section of the setup file: the spawn pad, the
   starting weapons and ammo, and the intro cameras.
4. `playerStartNewLife`: eye height, per-life counters, the fist.
5. `headBobInit`: sets up the hidden walk animation.

## Each frame

The level loop calls `playerUpdate` for each player. It sets the player's viewport, field
of view and options, then either:

* `playerMove`, in first-person modes. In order: damage and health (`playerTickDamageAndHealth`),
  input (`playerProcessInput`: sticks and buttons into turn, look, walk and strafe speeds,
  weapon switching, crouch and lean), the watch (`watchTick`), the tank if Bond is in it,
  moving with collision, the head bob animation (`playerTickMoveAnim`), and the floor
  height (`playerUpdateVertical`).
* `playerMoveFrozen`, in the intro, cutscenes and the death camera, where the camera runs
  on its own (`playerTickFrozenCamera`).

Later in the frame, for each view, `camSetupFrame` builds the camera matrices,
`propsTickPerPlayer` ticks the props for that view (see [props](props.md)), the hands fire
(`shotFireHands`), and `hudRender` draws the health bars, the watch, HUD messages and the
credits. Each player is also a prop of type `PROP_TYPE_PLAYER` with a guard-style body:
`playerTick` is its prop tick, and poses the body and its guns to match the player, so
other players see it in multiplayer and co-op, and the intro and death cameras show it.

## Camera modes

`g_CameraMode` (`playerSetCameraMode`) says who drives the camera:

| Mode | Camera |
| --- | --- |
| `CAMERAMODE_INTRO` | The level's intro cameras from the setup file |
| `CAMERAMODE_FADESWIRL`, `CAMERAMODE_SWIRL` | Swirl round Bond into his eyes (`playerCalcIntroSwirlCamera`) |
| `CAMERAMODE_FP` | First person: normal play |
| `CAMERAMODE_DEATH_CAM_SP`, `CAMERAMODE_DEATH_CAM_MP` | After death, looking at Bond's body (`playerPickDeathCamera`) |
| `CAMERAMODE_POSEND` | A cutscene camera picked by an AI script, or orbiting a pad (`g_OrbitCam*`) |
| `CAMERAMODE_FP_NOINPUT` | First person with input locked |
| `CAMERAMODE_MP` | Multiplayer start |
| `CAMERAMODE_FADE_TO_TITLE` | Fading out to the title screen |

`playerAdvanceCameraMode` steps to the next mode when one finishes. `playerSetCameraModeRaw`
only stores the mode; `playerSetCameraMode` also sets up the new mode.

## The camera helpers

`camview.c` keeps the current player's camera: screen size and position, field of view,
the scale factors between screen pixels and view space (`camUpdateScale`), and the matrices
that `camUpdateMatrices` makes each frame:

* `camGetWorldToViewMtx`: world to camera, fixed point, for the RSP.
* `camGetProjViewMtx`: projection times world to camera, fixed point, loaded as the
  projection matrix when drawing in world space.
* `camGetWorldToScreenMtxf`: world to camera, floats. Despite the name, it stops before
  the projection.
* `camGetViewToWorldMtxf`: the inverse, used to place things in front of the camera.
* `camGetPrev*`: last frame's matrices.

`camViewToScreen` and `camScreenToViewDir` convert between view space and screen pixels,
and `camUpdateFrustumPlanes` makes the planes used to test whether a prop is on screen.

Positions given to the RSP are relative to `current_model_pos` (`camSetRoomOrigin`), not
the world origin, so that the fixed-point matrices keep their precision.

## Moving

`playerMoveWithCollision` moves Bond by the frame's offset. It tries the full move
(`playerTryMoveSimple`); if that is blocked it tries a shorter move up to the wall
(`playerTryMoveFraction`), sliding along the wall's edge (`playerTryMoveAlongEdge`), and
stepping round the end of the edge (`playerTryMoveRoundCorner`). Each try uses the floor
tiles (see [nav](nav.md)) and the props' collision outlines (`playerTestMove`).
`playerUpdateVertical` then sets Bond's height from the floor under him, with falling and
crouching. `playerUpdateRooms` keeps the player's prop in the right rooms. All but
`playerUpdateVertical` are in `playermove.c`.

## The watch

Pressing Start brings up the watch: `watchTrigger` starts it, `watchTick` runs the raise
animation, the camera tilts down (`watchStartPauseTilt`) and zooms in (`watchZoomTo`), then
the watch menu (in `menu/`) takes over. `watchRender` draws Bond's arm and the watch. All
of this is in `watch.c`.
In solo play the game is paused while the watch is out; in co-op it pauses for both.

## Health and death

`bondhealth` and `bondarmour` run from 0 to 1. `playerTakeDamage` takes armour first
(unless the damage ignores armour, like gas), then health, and calls `playerDie` at 0.
`playerTickDamageAndHealth` runs the red flash and the health bars, and `playerTickExplode`
the explosions some levels set off round Bond (`playerSurroundWithExplosions`), all in
`playerdamage.c`. In
multiplayer `mpRespawnPlayer` respawns at a pad from `playerChooseSpawnPad`.

## The tank

When Bond drives the tank, `g_PlayerTankProp` is the tank. `playerMove` (`playerupdate.c`)
drives it; the `tank*` functions in `tank.c` test its collision box against the floor
tiles and props (`tankTestCollision`, `tankTryMoveTo`) and place and aim the turret
(`tankUpdateTurretOffset`, `tankGetTurretDir`).

## Co-op and counter-op

Added for this fork:

* `g_GameMode` (`level/gamemode.c`) is solo, co-op or counter-op, set on the mode select
  page. Test it with `gameIsCoop()` (co-op only), `gameIsCounterOp()` and
  `gameIsTwoPlayerMission()` (either). `gameIsMulti()` is true in a multiplayer match;
  it goes by the player count, since co-op also has two players.
* `coopUpdateSoloView` (`cammode.c`) decides each frame whether one player's view fills
  the screen.
* `coopRespawnPlayer` (`respawn.c`) respawns a dead player next to the partner, taking
  health from them. `hudRender` runs it when the dead player presses a button.
* `coopTankBeginPlayer` and `coopTankEndPlayer` (`tank.c`) let one player own the tank;
  the other walks, and the tank is solid for them (`coopTankIsHidden`, `playerTestMove`).
* `counterop.c` lets player 2 play as the guards: `counterOpTakeOverGuard` puts them in a
  guard out of Bond's sight, with that guard's body, guns and health, or where a dead guard
  first stood (`s_CounterOpStarts`); also the suicide pill and the end screen's stats.

## Traps

* "Bond" in a name often means "the current player", from when there was only one.
* `player->bond` (`struct PlayerBond`) is Bond's body: its tile, position and look
  vectors. Its position is the real player position; `bondDie` is a copy taken at death
  for the death replay.
* The player struct holds two whole models: `watchModel` (the arm and watch, whose
  animation frame is the watch open/close time) and `model` (the head bob animation), each
  followed by its node rwdata. It also holds the watch's display lists (`watchArmorGdl`,
  `watchTabsGdl` and so on), built once when the watch opens.
* Camera smoothing: `sumGround`/`manGround` ease the feet onto the ground when stepping
  or falling, `sumCrouch`/`crouchHeight` give the dip after landing, and
  `dampEyeSum`/`dampEye` ease the eye position. All use the same damped-sum pattern:
  `sum = sum * damp + value; smoothed = sum * (1 - damp)`.
* `getMissiontimer` is now `lvGetMissionTime60`; on PAL it converts 50 Hz ticks to 60 Hz.
