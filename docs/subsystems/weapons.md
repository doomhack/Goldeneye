# Weapons

Bond's guns and what he carries: the two hands and their gun models, switching, reloading
and firing, ammo, the inventory, thrown and launched projectiles, tracers, ejected casings
and the ammo HUD. The player as a whole is in [player](player.md).

| File | What it holds |
| --- | --- |
| `src/game/player/gun.c` | The hands: which gun each holds, loading its first-person model, switching and cycling weapons, reloading, zoom, the weapon stats table lookups (`weaponGet*`), sway, throwing grenades, knives and mines, grenade launcher rounds, rockets and tank shells; also the gun keyframe editor (`gunDeb*`, debug only) |
| `src/game/player/gunfire.c` | Each frame for the hands: the hand state machine (`gunTickHandState`), positioning and drawing the gun models, beams and tracers, hit sounds, the crosshair and aim, ejected casings, ammo, the ammo HUD and sight, per-player stats (`stats*`), and firing shots into the world (`shotFireHands`, `shotFireBullet`) |
| `src/game/player/bondinv.c` | The inventory (`inv*`): what the player carries, cycling through it, keys, text overrides for picked-up objects, and the watch's item list |
| `src/game/player/gun_rodata.c` | Four float constants kept in their own file so the linker keeps `gun.c`'s literal pool in order |

## Hands

The player has two hands, `GUNRIGHT` and `GUNLEFT`, each a `struct hand` in
`g_CurrentPlayer->hands`. The left hand is only used when dual wielding. A hand holds:

* the weapon it is using (`weaponnum`, read with `gunGetWeapon`), and the one the watch
  shows while it is open (`gunGetShownWeapon`);
* the action state (`weapon_action_state`, a `GUN_ANIM_STATE_*`): idle, firing, recoil,
  switching, reloading, lowering for the watch, the knife slashes;
* a requested next state (`weapon_current_animation`): `gunEquip` asks for
  `GUN_ANIM_STATE_SWITCH_LOWER`, `gunTryReload` for `RELOAD_START`, `gunStartWatchSwap`
  for `WATCH_LOWER`;
* the rounds in the magazine (`weapon_ammo_in_magazine`), sway, recoil and the gun's
  position on screen.

Each hand's gun model is loaded on demand into a buffer of its own (`gunLoadHandModel`).
`gunSetHandModel` asks for a new model, which is swapped in when the gun is lowered.

## Each frame

1. `playerProcessInput` (in [player](player.md)) calls `gunTickGameplay` with the trigger.
   For each hand it runs `gunTickHandState`, which advances the action state: fire on
   trigger, recoil, auto-fire at the weapon's rate, reload when empty, switch to the next
   weapon (`gunSwitchWhenEmpty`) when out of ammo. It also sets how much noise the gun
   made (`gunTickNoise`), which guards hear.
2. In the per-view loop (see [props](props.md)), `shotFireHands` fires the shots: a ray
   from the camera through the crosshair (`gunGetShotDir`, with spread) tested against
   props and the room geometry, then damage and impacts.
3. `hudRender` calls `gunUpdateAndFireBothHands`, which works out each gun's matrix from
   its keyframes, sway, recoil and the crosshair, creates beams and ejects casings; then
   it draws the casings (`casingsRender`), the gun models (`gunRenderHands`), the sight
   (`gunDrawSight`) and the ammo counters (`hudRenderAmmo`).

## Weapon stats

`weaponGetStats` returns the `WeaponStats` for an item: damage, fire rate, spread,
magazine size, ammo type, zoom, recoil, sounds, first-person position, and flags
(`weaponHasFlag`, `WEAPONSTATBITFLAG_*`). Guards' guns use the same table.

## Ammo

Spare ammo is per type, in `ammoheldarr` (`ammoGetHeld`, `ammoGive`, capped by
`ammoGetMax`). Each hand's magazine is separate. `gunReload` moves ammo from the spare
pool into the magazine; `ammoGetTotal` counts both.

## The inventory

The inventory is a circular, sorted, doubly linked list of `InvItem` (`invInsertItem`,
`invSort`). An item is a weapon (`INV_ITEM_WEAPON`), a pair of weapons for dual wielding
(`INV_ITEM_DUAL`), or a picked-up prop such as a key or a mission item (`INV_ITEM_PROP`).
`invCycleForward` and `invCycleBackward` step through it when the player changes weapon.
`invAllocate` sizes the list per level: 30 items plus one per linked prop in the setup.
Text overrides (`invAddTextOverride`) give picked-up objects their watch names. The watch's
inventory page reads item positions and names through `invGet*ForIndex` and `itemGet*`.

## Projectiles

Grenades, knives, mines, launcher rounds, rockets and tank shells are objects launched
from in front of the camera by `gunLaunchProjectileFromPlayer`, which falls back to the
player's position if the spot in front is inside a wall. The objects then fly on their
own (see [props](props.md)).

## Traps

* `weapon_current_animation` is the requested next state, not an animation.
* `gunGetCrosshairPos` returns a screen position, though the field is `crosshair_angle`.
* The `itemGetWatchPos*` names follow the item table's fields (`watch_pos_x`...), which
  the decomp named before anyone checked which axis each one is.
* `ammoGiveMaxAll` is the infinite ammo cheat, called every frame while it is on.
* Two unnamed functions in `gun.c`, `sub_GAME_7F05E978` and `sub_GAME_7F05EA94`, show or
  hide groups of switch nodes on a gun model; which parts they are is still unknown.
