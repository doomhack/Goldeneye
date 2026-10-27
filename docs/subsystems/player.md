# Player

Bond, or each player in multiplayer and co-op: movement and collision, the camera, the
watch and pause tilt, HUD messages, health and death, and the tank. Guns, firing and the
inventory are in [weapons](weapons.md).

| File | What it holds |
| --- | --- |
| `src/game/player/player.c` | The player list: `g_playerPointers`, the current player (`playerSetCurrent`), allocating each player (`playerInit`), the draw order, per-player screen size and field of view, the hands' first setup (`playerInitHands`) |
| `src/game/player/bondview.c` | The player globals, the camera helpers (`cam*`): screen and view transforms, the view matrices, the frustum planes; also choosing a spawn pad, resetting a player for a new life, crouch and lean |
| `src/game/player/bondview2.c` | Everything else about Bond, 12,000 lines: camera modes and cutscene cameras, the tank, moving with collision, the watch and pause tilt, input, health and damage, the per-frame update (`playerUpdate`, `playerMove`), the frame's camera setup (`camSetupFrame`), the HUD (`hudRender`), HUD messages, the player prop tick (`playerTick`), co-op respawn and counter-op |
| `src/game/player/bondview_r.c` | Level start for a player: the intro section of the setup file (`playerApplyIntro`), co-op spawn points, loading projectile models |
| `src/game/player/bondhead.c` | Head bob: the hidden walk animation whose root motion moves the camera up and down as Bond walks, and breathing |

## Players

A `struct player` (in `bondview.h`) holds everything about one player, about 11 KB of it.
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
5. `bheadInit`: sets up the hidden walk animation.

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

`bondview.c` keeps the current player's camera: screen size and position, field of view,
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
crouching. `playerUpdateRooms` keeps the player's prop in the right rooms.

## The watch

Pressing Start brings up the watch: `watchTrigger` starts it, `watchTick` runs the raise
animation, the camera tilts down (`watchStartPauseTilt`) and zooms in (`watchZoomTo`), then
the watch menu (in `menu/`) takes over. `watchRender` draws Bond's arm and the watch.
In solo play the game is paused while the watch is out; in co-op it pauses for both.

## HUD messages

`hudmsgBottomShow` shows a line at the bottom of the view (pickups, objectives), and
`hudmsgTopShow` one at the top. `hudmsgTopTick` and `hudmsgTickIntroText` time them out.

## Health and death

`bondhealth` and `bondarmour` run from 0 to 1. `playerTakeDamage` takes armour first
(unless the damage ignores armour, like gas), then health, and calls `playerDie` at 0.
`playerTickDamageAndHealth` runs the red flash and the health bars, and `playerTickExplode`
the explosions some levels set off round Bond (`playerSurroundWithExplosions`). In
multiplayer `mpRespawnPlayer` respawns at a pad from `playerChooseSpawnPad`.

## The tank

When Bond drives the tank, `g_PlayerTankProp` is the tank and the `tank*` functions in
`bondview2.c` move it, test its collision box against the floor tiles and props
(`tankTestCollision`), and aim the turret (`tankGetTurretDir`).

## Co-op and counter-op

Added for this fork, at the end of `bondview2.c`:

* `coopUpdateSoloView` decides each frame whether one player's view fills the screen.
* `coopRespawnPlayer` respawns a dead player next to the partner, taking health from them.
* `coopTankBeginPlayer` and `coopTankEndPlayer` let one player own the tank.
* `counterOpTick` and friends let player 2 take over a guard.

## Traps

* "Bond" in a name often means "the current player", from when there was only one.
* `bondview.c` is mostly camera code plus the player globals, and `bondview2.c` is most of
  the player. The names come from the decomp; splitting `bondview2.c` by topic (watch,
  HUD messages, tank, co-op) is left for later.
* Many fields of `struct player` are still `field_XXX`. `field_488` is the player's
  collision record (`struct collision434`): its position is the real player position.
* `getMissiontimer` is now `lvGetMissionTime60`; on PAL it converts 50 Hz ticks to 60 Hz.
