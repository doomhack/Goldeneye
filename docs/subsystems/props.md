# Props

Everything in a level that is not the room geometry, Bond or a guard: doors, crates, guns
on the floor, cameras, autoguns, vehicles, glass, plus the explosions and smoke they make.

| File | What it holds |
| --- | --- |
| `src/game/prop/prop.c` | The prop manager: the prop pool and lists, the per-frame tick and draw loops, room prop lists, collision outlines, auto-aim |
| `src/game/prop/setup.c` | Level setup: loading the setup file and creating every prop in it; setup lookup helpers; the per-level prop state reset |
| `src/game/prop/propobj.c` | Objects and doors at run time: movement and projectiles, the per-frame tick, drawing, damage, pickups, weapon and hat slots, doors, alarm, gas and the countdown timer |
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

1. `propsTickOnce` runs once per frame: guard AI (`chrsTick`), weapon respawns,
   tracers, and the explosion and smoke ticks.
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

A `DoorRecord` has a state (`DOORSTATE_STATIONARY`, `OPENING`, `CLOSING`, `WAITING`) and an
open fraction (`openPosition`). `doorsUpdatePositions` moves a door and the doors linked to
it, and `doorUpdateDisplacement` turns the fraction into a slide or a swing. Linked doors
(double doors, the Dam gates) move together, and `DOORSTATE_WAITING` waits for a sibling to
close. Opening a door turns on the room portal behind it
(`doorActivatePortal`), so the room is drawn; closing it turns it off again.
`doorsChooseSwingDirection` makes doors that swing both ways open away from the player.

## Monitors

Screens run a small animation program (`struct tvcmd`: scroll, scale, set image, wait,
loop...) to show scrolling images. `monitorTickAnimation` steps it each time the screen is
drawn. The programs are the `monAnim*` arrays near the top of `propobj.c`.

## Alarm, gas and the countdown timer

There is one alarm and one gas leak per level, started by AI scripts (`alarmActivate`,
`gasStart`) and ticked by `alarmGasTimerTick`, which also ticks the on-screen countdown
timer (`countdownTimer*`). `propsResetLevelState` clears all three on level load, and
`alarmsReset` turns off the alarm and stops the gas sound at level end.

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
