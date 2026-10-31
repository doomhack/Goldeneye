# Props

Everything in a level that is not the room geometry, Bond or a guard: doors, crates, guns
on the floor, cameras, autoguns, vehicles, glass, plus the explosions and smoke they make.

| File | What it holds |
| --- | --- |
| `src/game/prop/prop.c` | The prop manager: the prop pool and lists, the per-frame tick and draw loops, respawning, the use button |
| `src/game/prop/proprooms.c` | Which props are in which rooms: the room prop lists |
| `src/game/prop/propcollision.c` | Collision between props on the x/z plane: outlines, circles, CDTYPE filtering |
| `src/game/prop/bbox.c` | Model bounding boxes turned into the world, and their x/z outlines |
| `src/game/prop/autoaim.c` | Choosing the auto-aim target |
| `src/game/prop/setup.c` | Level setup: loading the setup file, the two passes over its propdefs, setup lookup helpers, the per-level prop state reset |
| `src/game/prop/objsetup.c` | Creating each kind of object from its setup record, at its pad or bound pad |
| `src/game/prop/propobj.c` | Objects at run time: creating their props and models, placing them at a pad, freeing them |
| `src/game/prop/objrender.c` | Drawing objects and doors, whether things are in view, and object shading |
| `src/game/prop/objtick.c` | The per-frame tick of objects and doors |
| `src/game/prop/cctv.c` | Security cameras |
| `src/game/prop/autogun.c` | Autoguns (drone guns) |
| `src/game/prop/vehicle.c` | Vehicles, aircraft, and the tank's model |
| `src/game/prop/objdamage.c` | Damage to objects: destroying, exploding, bending the model, what being destroyed does |
| `src/game/prop/objhit.c` | Rays against object models (`modelFindTriangleHit`), the player's shots hitting objects |
| `src/game/prop/projectile.c` | Objects in flight: the projectile pools, moving with collision, bouncing, settling, sticking |
| `src/game/prop/pickup.c` | Picking props up and using them with the action button |
| `src/game/prop/objslots.c` | Slots for objects made during play: weapons, hats and ammo crates |
| `src/game/prop/carried.c` | Guns and hats that characters carry, and dropping them |
| `src/game/prop/mine.c` | Live explosives: grenades, rockets, plastique, and remote and proximity mines |
| `src/game/prop/door.c` | Doors: moving, collision, portals, sounds, and the player using them |
| `src/game/prop/monitor.c` | Monitor screens: running their scripts and drawing them |
| `src/game/prop/monitorscripts.c` | The monitor scripts |
| `src/game/prop/alarm.c` | The alarm, the gas leak and the countdown timer |
| `src/game/prop/explosion.c` | Explosions: fireballs, damage, screen shake |
| `src/game/prop/smoke.c` | Smoke left by explosions, and rocket and grenade trails |
| `src/game/prop/debris.c` | Rubble thrown out by explosions and bullet hits |
| `src/game/prop/decal.c` | Scorch marks and bullet impacts on walls and objects |
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
4. `shotFireHands` (in `player/shot.c`) fires the player's guns, `propsTickPickups`
   checks what the player is touching, and `playerInteract` handles the use button.

## Room prop lists

Each room has a list of the props in it, kept in blocks of 15 prop numbers
(`g_RoomPropBlocks`, 256 blocks shared by all rooms, chained per room from
`g_RoomFirstPropBlock`). `propAddToRooms` and `propRemoveFromRooms` keep them up to date
as props move, and `propsDefragRoomProps` compacts them. `roomGetProps` collects the props
in a list of rooms into `g_RoomPropsFound`, which is how collision, line-of-sight and shot
tests find nearby props without looking at all 600.

## Collision between props

Props collide on the x/z plane, each with a height range (`propcollision.c`). Objects and
doors use a convex outline: the hull of their bounding box's corners seen from above
(`bboxGetHull2d` in `bbox.c`), worked out when they are placed or move
(`objUpdateCollisionBounds`). Guards and players are circles. `propIsOfCdType` says
whether a prop counts for a test's `CDTYPE` flags, and `propSetMoving(prop, FALSE)` takes
a prop out of tests for a moment (so a guard's line of sight does not hit the guard).
`propIsUnblocked` tells a moving door or vehicle whether anything is in its way.

## Respawning

A used-up object that respawns (`PROPSTATE_RESPAWN`) is hidden by
`propExecuteTickOperation` for 1200 ticks (1000 on PAL). `propsTickOnce` counts it down;
it comes back for the last `CHROBJ_TIMETOREGEN` ticks and fades in, but waits while a
player is within 400 units.

For doors, objects and weapons, the record behind the prop is an `ObjectRecord` (or a
type that starts with one: `DoorRecord`, `WeaponObjRecord`, `MonitorObjRecord`...). It
holds the model instance, the setup flags, the damage taken, and run-time state
(`runtime_bitflags`, `state`).

## Level setup

Each level has a setup file: pads (named positions), the path graph, AI lists, patrol
paths, and the propdef list, which describes every prop. `setupCreateProps`:

1. loads the setup file and turns its offsets into pointers (`setupRebasePointers`);
2. counts the propdefs of each type to size the model slots
   (`modelmgrAllocateModelSlots`), the guard pool and the inventories;
3. walks the propdefs and creates each guard and object (`setupCreatePropdef`):
   `setupObject` for plain objects, and `setupDoor`, `setupWeapon`, `setupCctv`,
   `setupAutogun`, `setupSingleMonitor`, `setupTank` and so on (all in `objsetup.c`)
   for the types that need more. Objects whose flags2 has the "don't load" bit for the
   current difficulty (or multiplayer player count) are skipped;
4. walks them again to link records that refer to others (`setupLinkPropdef`): objects
   inside other objects, dual weapons, switches, padlocked doors and items in safes. The
   switch, padlock and safe links are chained into lists the tick code checks.

The propdefs are used in place. An `ObjectRecord` is the setup file's own record, filled in
at run time, so the prop data for the whole level lives in the setup file in memory.
Records refer to each other by index in the list, usually relative to their own:
`setupCommandGetObject` finds an object by index, which is also how AI scripts refer to
objects. `setupFreeProps` frees them all at level end.

Fractions and angles are stored as 16.16 fixed point in fields that are floats at run
time, and converted in place when the record is set up.

### Placing an object

A plain pad gives a position and a facing (`look` and `up`). A bound pad also gives a box
in pad space (x across, y up, z along look). An object at a bound pad stands at the box's
centre, unless `PROPFLAG2_ATPADPOS` puts it at the pad itself, and the `PROPFLAG_FITPAD*`
flags scale it to fill the box. The setup flags then choose how `objPlaceAtPad` puts it
there: on the floor (the default), `PROPFLAG_UPSIDEDOWN` hanging from the pad,
`PROPFLAG_INAIR` at the pad's height, or `PROPFLAG_SIDEWAYS` turned 90 degrees
(`objPlaceAtPadSideways`). Doors always use a bound pad and are scaled to fill it.

## Moving objects

An object that moves (thrown, dropped, knocked off a table, blown up) gets a `Projectile`
from a pool of 20 (`objEnsureProjectile`); when the pool is full, the oldest one is taken
over and its object removed. Each frame `objTick` calls `projectileTick`
(`projectile.c`):

1. A gun's projectile first moves straight out to its start point
   (`PROJECTILEFLAG_MOVETOTARGET`).
2. In flight (`PROJECTILEFLAG_AIRBORNE`) it falls (rockets rise, then fly under thrust),
   tumbles, and moves. With `PROJECTILEFLAG_COLLIDE3D` the move is tested in 3D against
   the level and the props in the rooms along the way (`projectileMove`); otherwise only
   along the floor plan (`objMoveAlongFloor`).
3. What it hit decides the rest: mines, bugs and plastique stick (attached to a prop
   with an `Embedment`, from a pool of 40); a throwing knife shoots a guard; a rocket or
   grenade round goes off. Anything else bounces, and once a bounce is slow it settles
   (`objSettle`).
4. Landed, it turns to lie flat and slides to a stop, and then loses its projectile.

The tick goes through the projectile code once per frame, on the player who threw it.

Objects that do not move only need placing once: `objPlaceAtPad` puts an object on its
pad, `objPlaceOnFloor` drops it to the floor below, and `objGetTileShading` lights it with
the colour of the tile it is on.

## The object tick

`objTick` (`objtick.c`) is the tick for every object and door, on every player's pass.
An object's own behaviour runs once a frame, on the "sim owner" pass: the first player in
the frame's shuffled order, or for a projectile in flight the player who threw it. That
part moves projectiles (`projectileTick`), and runs doors (`doorTick`), security cameras
(`cctvTick`), autoguns (`autogunTick`), vehicles (`vehicleTick`) and aircraft
(`aircraftTick`). The rest runs on every pass, as it is per view: the opacity of tinted
glass, whether the object is in view, and its model's matrices (`objGetModelMatrices`,
which calls the type's own: `cctvGetModelMatrices`, `vehicleGetModelMatrices` and so on).
After that, the sim owner pass fires autoguns (`autogunTickFire`) and drops anything
attached to a destroyed object.

Every object, carried or in view or not, also goes through `objTickExplosive`
(`mine.c`), which counts down live grenades and mines. Attached props are ticked through
`objTickAttached`, and props out of view do the minimum through `objTickOffscreen`.

A security camera sweeps its head between two yaws and sets off the alarm if it sees a
player for 5 seconds (10 on Agent). An autogun turns towards a player in range and in sight,
fires while it is aimed close, and keeps firing for 2 seconds after losing sight; damage
builds up while it is aimed right at the player. A vehicle drives along the path its AI
script gives it, turning towards each waypoint, and tilts to fit the floor under its
wheels. In co-op cameras watch every living player, and an autogun keeps the nearest one
as its target.

`objTickPlayer` and `weaponTickPlayer` (`pickup.c`), called from `propsTickPickups`, check
whether a player is touching a prop and can pick it up.

## Drawing objects

`propRender` calls `objRenderProp` (`objrender.c`) for each object, weapon or door, once
for the opaque pass and once for the translucent pass. The object fades out in near fog
(`propGetFogFade`) and while it is about to respawn; a faded object, or one flagged to
always draw in front, is drawn whole on the translucent pass. The drawing is scissored to
the screen area of the rooms the prop is in (`propGetRoomsBbox2d`). Its shade colour
(from the floor it stands on, `objGetShadeColour`) is darkened for each shot it has taken
and tinted towards the fog colour (`shadingLerp`).

`objRenderModel` then draws monitor screens (`monitorTickAndDraw`), the model, bullet
impacts on it, and its children. Doors draw with the cull mode for the side they open to.
A damaged object whose vertices have been bent draws both sides of each face, shaded
darker the more damaged it is. After the translucent pass the model's matrices are
converted to fixed point in place, as the display lists point at them.

`posIsOnScreen` decides whether a prop is in view for the ticks: one of its rooms is
visible, it is not hidden by fog, it is in the screen area of its rooms, and it is within
32000 units. `weaponRenderTracers` draws the gun beams of guards, drone guns and the other
players.

## Damage

An object has `health` (from its setup) and `damageTaken`. `objApplyDamage`
(`objdamage.c`) adds 250 times the weapon's damage; when that passes the health,
`objExplode` destroys it (`PROPSTATE_DESTROYED`), makes an explosion and knocks off what
stands on it (`objDestroySupportedObjects`). After that, `damageTaken` counts shots
instead, and every four shots move it to the next destroyed level, up to 3
(`objGetDestroyedLevel`); after 12 it is wrecked (`RUNTIMEBITFLAG_WRECKED`) and shots go
through it. Each level is drawn more broken: `objDeform` bends and blackens the model's
vertices in a copy taken from the vertex store (see [rendering](rendering.md)), with
random numbers seeded per vertex so an object breaks the same way each time. Destroying an
object can set off its children (`objDetonateRecursive`).

The player's shots are tested against objects by `objTestShot` and applied by `objHit`
(`objhit.c`), which also knocks light objects into the air.

Glass breaks with `glassShatter`, which hides the pane and fills its area with shards.

## Pickups

Each frame `objTickPlayer` (`pickup.c`) checks whether the current player is touching a
prop: within 1 m across and 2 m up or down, on the same tile and with nothing in the way
(unless the prop has `PROPFLAG2_PICKUPTHROUGHWALLS`). With the watch magnet on for a
second the range is 3.5 m across and 5 m up or down. A player looking more than 45 degrees
down picks nothing up, unless the magnet is on. Props the player would gain nothing from are
left lying: a gun they have with full ammo (unless it makes a pair they haven't got), ammo
they can't carry (unless it gives an item they haven't got, such as grenades), armour no
better than theirs, and live grenades and mines.

`propPickupByPlayer` collects a weapon, ammo, armour, key or other item: it adds it to the
inventory, plays the sound (`pickupPlayWeaponSound`) and shows the "Picked up..." text
(`pickupShowWeaponText`), or the text the level set up for the prop. Weapons that are
really ammo, such as grenades and mines, are worded as ammo. In solo, ammo pickups of more
than one round are multiplied by the difficulty's ammo multiplier.

Props flagged `PROPFLAG_PICKUPONINTERACT`, alarms, and switches linked to doors
(`RUNTIMEBITFLAG_LINKEDSWITCH`) are used with the action button instead:
`objTestForInteract` finds the one the player is facing, within 2 m and 22.5 degrees, and
`objInteract` uses it.

## Weapon and hat slots

Guns that appear during play (dropped by guards, thrown, placed mines) need props that
were not in the setup file. They come from 30 weapon slots (`g_WeaponSlots`), 10 hat slots
and 20 ammo crates (`objslots.c`). `weaponCreate` takes an empty slot if it can, then a
setup collectable that can be reused (`setupFindObjForReuse`), then a gun lying out of
view, then any gun lying on the floor; `hatCreate` does the same for hats.

## Carried objects and dropping

Guards' guns and hats (`carried.c`) are objects whose props are children of the guard's
prop, with their models attached to the right hand, left hand or head node. Setup guns are
put in hand by `chrSetupHeldWeapon`, and guns given during play by `chrGiveWeaponEx`;
`chrRenderHeldWeapon` works out the held gun's matrices each frame. A gun flagged
`PROPFLAG_IS_DOUBLE` is paired with the gun in the other hand (`weaponSetDual`).

`objDrop` drops a carried object as a projectile, launched by its drop type
(`objSetDropType`): a surrendering guard lets go of their gun in front of them, a thrown
grenade flies forwards and up, a shot-off hat flies away from the player, and anything
else tumbles off in a random direction. It starts where its model was drawn if it can
reach that point, otherwise at the guard's position.

## Mines and other live explosives

Thrown grenades, launcher rounds, rockets, plastique and placed mines are weapon objects
whose timer `objTickExplosive` counts down. Grenades and timed mines go off when it runs
out. A placed mine's timer counts down while it arms, then 1 means primed and 0 that it
goes off. Remote mines go off when their owner uses the
detonator (`remoteMinesDetonate`). Primed proximity mines are kept in a list of 30
(`g_ProxMines`, `mine.c`) so that guards and moving objects (thrown, falling or driven) can
set them off as well as the player (`proxminesDetonateNear`, within 2.5 m).

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

Screens (`MonitorObjRecord`, `MultiMonitorObjRecord` with four, and the screen on the
taser) each run a small script that scrolls, zooms, rotates and tints
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
for it, so it is listed, ticked and drawn like any other prop. The explosion type
(`EXPLOSION_DEF`, table in explosion.c) sets its size, duration, damage, sound, smoke and
debris. Each tick it adds and plays fireballs (up to 40), and four times over its life it
damages what is in range (`explosionInflictDamage`). It throws out debris as it starts
(`debrisCreate`), leaves smoke near its end (`smokeCreate`, 20 buffers,
`PROP_TYPE_SMOKE`) and, if asked, a scorch mark halfway through (`scorchCreate`, solo
only, 20). In multiplayer, a new explosion or smoke makes the existing smoke stop making
clouds (`smokesEndEarly`).

Debris (200, shared by all players) and glass shards are not props: lv.c ticks and draws
them each frame.

Bullet impacts on walls and objects are a ring buffer of 100 (`impactCreate`), and the
ten oldest fade out. Impacts on an object move with it; those on glass go when it
breaks (`impactsRemoveFromProp`) and all go when the object is deformed
(`impactsRemoveTranslucent`). `PROPSTATE_HASIMPACTS` and `PROPSTATE_HASXLUIMPACTS` say
whether an object has any to draw in each pass.

The camera shake is set to 6 ticks by an explosion and counted down by
`explosionScreenShake`.

## Glass shards

`glassCreateShards` fills a broken pane with small triangles. They come from a buffer of
200 shared by all players (fewer in split screen, halved on Streets and Depot), reused
oldest first, and vanish after 150 ticks.

## Traps

* `PROPFLAG_*` names are used for two different fields. On `PropRecord.flags` (8 bits),
  `PROPFLAG_ONSCREEN` and `PROPFLAG_ENABLED` mean what they say. On `ObjectRecord.flags`
  (the setup flags), the low bits mean how the object is placed. Those have their own
  names (`PROPFLAG_SIDEWAYS`, `PROPFLAG_UPSIDEDOWN`, `PROPFLAG_FITPADX`...) next to the
  older ones, which came from Perfect Dark.
* In `ObjectRecord`, `damage` is the object's health and `maxdamage` the damage taken so
  far: the names are the wrong way round.
* The `chrobj*` functions are object code, not character code; the prefix comes from the
  decomp.
* `objTick` mixes the tick for every object type in one function. Splitting it is Phase 2
  work.
* `prop.c` used to be `chr/chrprop.c`, and the setup code used to be `prop.c`. Old notes
  and Perfect Dark comparisons may use the old names.
