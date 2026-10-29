# Props

Everything in a level that is not the room geometry, Bond or a guard: doors, crates, guns
on the floor, cameras, autoguns, vehicles, glass, plus the explosions and smoke they make.

| File | What it holds |
| --- | --- |
| `src/game/prop/prop.c` | The prop manager: the prop pool and lists, the per-frame tick and draw loops, room prop lists, collision outlines, auto-aim |
| `src/game/prop/setup.c` | Level setup: loading the setup file and creating every prop in it; setup lookup helpers; the per-level prop state reset |
| `src/game/prop/propobj.c` | Objects at run time: movement and projectiles, the per-frame tick, drawing, damage, pickups, weapon and hat slots |
| `src/game/prop/door.c` | Doors: moving, collision, portals, sounds, and the player using them |
| `src/game/prop/monitor.c` | Monitor screens: running their scripts and drawing them |
| `src/game/prop/monitorscripts.c` | The monitor scripts |
| `src/game/prop/alarm.c` | The alarm, the gas leak and the countdown timer |
| `src/game/prop/explosion.c` | Explosions, smoke, debris particles, scorch marks, bullet impacts, screen shake |
| `src/game/prop/glass.c` | Glass shards from broken windows |

## Props and objects

A `PropRecord` is the thing the engine tracks: a type (`PROP_TYPE_OBJ`, `DOOR`, `CHR`,
`WEAPON`, `PLAYER`, `EXPLOSION`, `SMOKE`), a pointer to the record for that type, a
position, the floor tile it is on (`stan`) and the rooms it is in. Props can be attached
to each other (`parent`, `child`): a gun held by a guard, a crate inside another crate, a
mine stuck to a door.

There are 600 props in one pool (`g_Props`), on a free list that `propsInit` rebuilds each
level. `propAllocate` takes one, `propActivate` adds it to the active list, and `propFree`
gives it back.

## Each frame

1. `propsTickOnce` runs once per frame: the level's background AI scripts (`bgChrsTick`),
   weapon respawns, tracers, and the explosion and smoke ticks. Guards' own AI runs in
   `chrTick`, on the first per-player pass.
2. For each player's view, `propsTickPerPlayer` ticks every active prop (`chrTick`,
   `objTick`, `explosionPropTick`, `smokePropTick`, `playerTick`). Each tick also decides
   whether its prop is on screen, and returns a `TICKOP` saying whether to keep, free or
   disable the prop (`propExecuteTickOperation`).
3. `propsSortOnscreen` sorts the props on screen by depth, and `propsRenderPass` draws
   them room by room.
4. `shotFireHands` (in `player/gunfire.c`) fires the player's guns, `propsTickPickups`
   checks what the player is touching, and `playerInteract` handles the use button.

## Room prop lists

Each room has a list of the props in it, kept in blocks of 15 prop numbers
(`g_RoomPropBlocks`, 256 blocks shared by all rooms, chained per room from
`g_RoomFirstPropBlock`). `propAddToRooms` and `propRemoveFromRooms` keep them up to date
as props move, and `propsDefragRoomProps` compacts them. `roomGetProps` collects the props
in a list of rooms into `g_RoomPropsFound`, which is how collision, line-of-sight and shot
tests find nearby props without looking at all 600.

For doors, objects and weapons, the record behind the prop is an `ObjectRecord` (or a
type that starts with one: `DoorRecord`, `WeaponObjRecord`, `MonitorObjRecord`...). It
holds the model instance, the setup flags, the damage taken, and run-time state
(`runtime_bitflags`, `state`).

## Level setup

Each level has a setup file: pads (named positions), the path graph, AI lists, patrol
paths, and the propdef list, which describes every prop. `setupCreateProps`:

1. loads the setup file and turns its offsets into pointers;
2. counts the propdefs of each type (`setupCountPropdefs`) to size the model slots
   (`modelmgrAllocateModelSlots`) and the guard pool;
3. walks the propdefs and creates each prop: `setupObject` for plain objects, and
   `setupDoor`, `setupWeapon`, `setupCctv`, `setupAutogun`, `setupSingleMonitor` and so on
   for the types that need more. Props with the "don't load on this difficulty" flag for
   the current difficulty are skipped.

The propdefs are used in place. An `ObjectRecord` is the setup file's own record, filled in
at run time, so the prop data for the whole level lives in the setup file in memory.
`setupCommandGetObject` finds a record by its index in the list, which is how AI scripts
refer to objects. `setupFreeProps` frees them all at level end.

Links between props (switches, locked doors, items inside safes) are separate propdefs.
`setupLinkSwitch`, `setupLinkLockDoor` and `setupLinkSafeItem` chain them into lists that
the tick code checks.

## Moving objects

An object that moves (thrown, dropped, knocked off a table, blown up) gets a `Projectile`
from a pool of 20 (`objEnsureProjectile`). `objTick` moves it each frame with
`projectileMove`, which tests the move against the floor tiles and other props
(`projectileFindCollidingProp`) and bounces or stops it. When it comes to rest,
`objSettle` lines it up with the floor.

An object stuck to something (a knife in a wall, a mine on a door) gets an `Embedment`
from a pool of 40 instead.

Objects that do not move only need placing once: `objPlaceAtPad` puts an object on its
pad, `objPlaceOnFloor` drops it to the floor below, and `objGetTileShading` lights it with
the colour of the tile it is on.

## The object tick

`objTick` is the tick for every object and door, and is about 2,400 lines long. It handles
falling and flying, security cameras spotting Bond, autoguns aiming and firing, vehicles
and aircraft following their paths, the tank, doors opening and closing, and weapons such
as mines (`chrobjWeaponTick`). Attached props are ticked through `objTickAttached`, and
props out of view do the minimum through `objTickOffscreen`.

`objTickPlayer` and `weaponTickPlayer`, called from `propsTickPickups`, check whether a
player is touching a prop and can pick it up.

## Damage

`objTestShot` tests a shot against a prop's model. `objApplyDamage` adds the damage to the
total taken, and when that reaches the object's health, `objExplode` destroys it
(`PROPSTATE_DESTROYED`) and makes an explosion. After that, the total counts shots
instead, and every four shots move it to the next destroyed level, up to 3
(`objGetDestroyedLevel`). Each
level is drawn more broken: `objDeform` bends the model's vertices in a copy taken from the
vertex store (see [rendering](rendering.md)), and the model is drawn darker. Destroying an
object can destroy what stands on it (`objDestroySupportedObjects`) and set off its
children (`objDetonateRecursive`).

Glass breaks with `glassShatter`, which hides the pane and fills its area with shards.

## Pickups

`propPickupByPlayer` collects a weapon, ammo, key, hat or other item: it adds it to the
inventory, plays the sound (`pickupPlayWeaponSound`, `pickupPlayAmmoSound`) and shows the
"Picked up..." text (`pickupShowWeaponText`, `pickupShowAmmoText`).

## Weapon and hat slots

Guns that appear during play (dropped by guards, thrown, placed mines) need props that
were not in the setup file. They come from 30 weapon slots (`g_WeaponSlots`), 10 hat slots
and 20 ammo crates. `weaponCreate` takes an empty slot if it can, then a setup
collectable that can be reused (`setupFindObjForReuse`), then a gun lying out of view,
then any gun lying on the floor. Placed proximity mines are also kept in a list of 30
(`proxmineAdd`), which `proxminesTickGuards` checks against guards.

Guards' held guns are set up by `chrSetupHeldWeapon` and `chrGiveWeaponEx`.

## Doors

Doors are in `door.c`. A `DoorRecord` has a state (`DOORSTATE_STATIONARY`, `OPENING`,
`CLOSING`, `WAITING`) and how far open it is (`openPosition`, from 0 to `maxFrac`: a
distance for a sliding door, an angle in degrees for a swinging one). `doorGetMtx` turns
that into a slide or a swing about the hinge.

`doorMove` opens or closes a door and the doors linked to it (double doors, the Dam gates),
which move together. `doorToggle` is someone using it. `doorTick`, from `objTick`, closes a
door that has been open for its `autoCloseFrames`, then moves it and its linked doors a
tick; if any of them would hit something, they all go back. An airlock door
(`PROPFLAG2_DOOR_AIRLOCK`) closes the linked doors first and waits (`DOORSTATE_WAITING`)
until they have shut.

Each door keeps a collision outline from its model's bounding box, which it loses once it
is open past `perimFrac`. Doors that clip to their bounding box (`DOORFLAG_CLIP_TO_BBOX`)
also have their model cut back as they slide into the wall. Opening a door turns on the
room portal behind it (`doorOpenPortal`), so the rooms behind are drawn; closing it turns
it off again.

The player uses a door when near it and looking at it (`doorTestForInteract`, then
`doorInteract`), unless it is locked: `keyflags` names the keys that open it, and a locked
door can be set to open from one side. `doorChooseSwingDirection` makes doors that swing
both ways open away from whoever opens them.

A door's `soundType` picks its sounds from a table: one as it starts to move, a loop while
it moves, and one when it stops. Hinged types play only the opening and the shutting sound.

## Monitors

Screens (`MonitorObjRecord`, `MultiMonitorObjRecord` with four, and the screens on the
rocket launcher and taser) each run a small script that scrolls, zooms, rotates and tints
an image (`monitor.c`). A script is an array of words: a command (`enum MonitorCmd`) and
its arguments, written with the `MON_*` macros in `monitorscripts.c`.

`monitorTickAndDraw` runs the script each time the screen is drawn, until a command waits,
then moves each value (`MonitorTween`) a step towards its target and draws the image on
the model's screen node. Setup files and AI scripts pick one of 52 numbered scripts
(`monitorInit`, `monitorSetScript`); a destroyed screen switches to the black one.

Scripts can jump to another at random (`MON_GOTO_CHANCE`), which takes a random number,
so whether a screen is drawn changes the rest of the game.

## Alarm, gas and the countdown timer

There is one alarm, one gas leak and one countdown timer per level (`alarm.c`), started by
AI scripts and ticked once a frame by `alarmGasTimerTick`. `alarmGasTimerReset` clears them
at level load, and `alarmGasEndLevel` stops their sounds at level end.

* The alarm sounds for 30 seconds from when it is first set off (`alarmActivate`).
* The gas (`gasStart`) thickens the fog over 60 seconds, or 2 in Egypt, where it is
  harmless. Harmful gas makes Bond cough after 10 seconds and hurts him after 30.
* The countdown timer counts down in ticks and is drawn at the bottom of the screen. It
  is shown only when nothing hides it: each reason (`enum CountdownHideReason`: the AI
  script, the watch, the red damage flash, a cutscene, a plastique blast) sets its own bit.

## Explosions and smoke

`explosionCreate` takes one of 6 explosion buffers and makes a `PROP_TYPE_EXPLOSION` prop
for it, so it is listed, ticked and drawn like any other prop. Each tick it grows its
parts, damages what is in range (`explosionInflictDamage`), and leaves smoke
(`explosionCreateSmoke`, 20 buffers, `PROP_TYPE_SMOKE`), a scorch mark (solo only, 20) and
debris (`explosionInitFlyingParticles`).

Bullet impacts on walls and props are a ring buffer of 100 (`explosionCreateBulletImpact`).
Impacts on a prop move with it and are removed when the prop is deformed.

The camera shake is `g_ShakeTicks`, set to 6 by an explosion and counted down by
`explosionScreenShake`.

## Glass shards

`glassCreateShards` fills a broken pane with small triangles. They come from a buffer of
200 shared by all players (fewer in split screen, halved on Streets and Depot), reused
oldest first, and vanish after 150 ticks.

## Traps

* `PROPFLAG_*` names are used for two different fields. On `PropRecord.flags` (8 bits),
  `PROPFLAG_ONSCREEN` and `PROPFLAG_ENABLED` mean what they say. On `ObjectRecord.flags`
  (the setup flags), the same bits mean something else: 0x2 is "rotated 90 degrees,
  upside down" and 0x4 "upside down", as the comments next to the enum say. The names
  came from Perfect Dark.
* In `ObjectRecord`, `damage` is the object's health and `maxdamage` the damage taken so
  far: the names are the wrong way round.
* The `chrobj*` functions are object code, not character code; the prefix comes from the
  decomp.
* `objTick` mixes the tick for every object type in one function. Splitting it is Phase 2
  work.
* `prop.c` used to be `chr/chrprop.c`, and the setup code used to be `prop.c`. Old notes
  and Perfect Dark comparisons may use the old names.
