# Source layout

The game code lives in `src/game`, one folder per subsystem. The engine and platform code
(boot, threads, scheduler, memory pools, audio, controllers, video) is in `src/`, and
Nintendo's SDK is in `src/libultra` and `src/libultrare`.

Include game headers by their path from `src/`, for example `#include "game/chr/chrai.h"`.

Each subsystem gets its own page in [docs/subsystems](subsystems) as it is cleaned up.

Many file names still come from the decomp, which named files after ROM addresses or a guess
at their contents, so a name doesn't always match what is inside. Those files are listed
under "Misleading names" below and get renamed as each subsystem is cleaned up.

## US and PAL

The same sources build the US (`make VERSION=US`) and the PAL (`make VERSION=EU`) ROM. Code
that differs between them sits behind these flags, which the Makefile sets:

| Flag | Build | Meaning |
| --- | --- | --- |
| `VERSION_US`, `VERSION_EU` | one each | Which build this is |
| `REFRESH_PAL` | PAL | 50 Hz timing |
| `BUGFIX_R0` | US | Code and data only the US release has |
| `BUGFIX_R1`, `BUGFIX_R2` | PAL | Fixes Rare made for the PAL release that the US build doesn't take |
| `BACKPORT_R1` | US | PAL fixes taken into the US build, where the US code differs from PAL's |

A PAL fix that both builds take is plain code with no flag around it.

## Assembly

All the game's own code is C. What is still assembly is code that cannot be C:

| File | Why |
| --- | --- |
| `src/rom_header.s` | The cartridge header: data, not code |
| `src/bootcode.s` | Nintendo's boot code (IPL3), which the console checksums |
| `src/_start.s` | The entry point: clears .bss and sets up `$gp` and the stack before any C can run, then jumps to `init()` |
| `src/rspboot.s`, `src/gspboot.s`, `src/aspboot.s`, `rsp/` | Microcode for the RSP, a different processor |
| `src/libultra`, `src/libultrare` | Nintendo's SDK, kept as it is: CP0, cache and TLB instructions and its own maths |

## Folders, from the top of the frame down

| Folder | What it does | Main files |
| --- | --- | --- |
| `level/` | Level lifetime: stage load, the per-frame tick and render, the game timers (`lv.c`), frame timing (`frametiming.c`), one-time game init (`game.c`). See [subsystems/level.md](subsystems/level.md) | `lv.c`, `frametiming.c`, `game.c` |
| `menu/` | Front end and in-game menus: the front-end menus and multiplayer setup (`front.c`), the intro sequence (`title.c`), cheats (`cheat.c`), multiplayer weapon sets (`mp_weapon.c`), the watch (pause) menu and per-player options (`watchmenu.c`), the multiplayer pause menu (`mpmenu.c`). See [subsystems/menu.md](subsystems/menu.md) | `front.c`, `title.c`, `cheat.c`, `mp_weapon.c`, `watchmenu.c`, `mpmenu.c` |
| `save/` | Save files in the cartridge EEPROM: folders, completion, best times, cheats, settings. See [subsystems/save.md](subsystems/save.md) | `file.c` |
| `player/` | Bond: the player list (`player.c`), camera helpers and player globals (`bondview.c`), movement, collision, camera modes, the watch, HUD and health (`bondview2.c`), level start (`bondview_r.c`), head bob (`bondhead.c`); inventory, guns and firing (`bondinv.c`, `gun.c`, `gunfire.c`). See [subsystems/player.md](subsystems/player.md) and [subsystems/weapons.md](subsystems/weapons.md) | `player.c`, `bondview.c`, `bondview2.c`, `bondinv.c`, `gun.c`, `gunfire.c` |
| `hud/` | On-screen overlays: radar (`radar.c`), health and armour bars and the watch screen shapes (`healthbar.c`), the death blood effect (`blood.c`), bullet sparks (`sparks.c`). See [subsystems/hud.md](subsystems/hud.md) | `radar.c`, `healthbar.c`, `blood.c`, `sparks.c` |
| `chr/` | Guards and other characters: slots and the per-frame tick (`chr.c`), actions such as patrolling, attacking and dying (`chraction.c`), the AI script interpreter (`chrai.c`), the global AI scripts (`chraidata.c`), bodies and heads (`body.c`). See [subsystems/guards.md](subsystems/guards.md) | `chr.c`, `chraction.c`, `chrai.c`, `body.c` |
| `prop/` | Props: the prop pool and per-frame tick and draw loops, level setup, doors, weapons on the floor, vehicles, monitor screens, explosions, glass, the alarm, gas and countdown timer. See [subsystems/props.md](subsystems/props.md) | `prop.c`, `setup.c`, `propobj.c`, `door.c`, `monitor.c`, `alarm.c`, `explosion.c`, `glass.c` |
| `nav/` | Where things can stand and walk: the floor tiles (`stan.c`), the collision tests that walk them (`stanwalk.c`), the floor-plane geometry under both (`geometry.c`), and the waypoint graph guards path along (`waypoint.c`). See [subsystems/nav.md](subsystems/nav.md) | `stan.c`, `stanwalk.c`, `geometry.c`, `waypoint.c` |
| `mission/` | Objectives and briefings: the lists built from the setup file, objective status and the status messages. See [subsystems/mission.md](subsystems/mission.md) | `objective.c` |
| `world/` | The level geometry: rooms and portals, visibility and drawing the rooms (`bg.c`), the room matrix cache (`roommtx.c`), fog (`bgfog.c`), sky (`sky.c`), water (`water.c`), light fixtures (`lightfixture.c`), what each kind of surface does when shot (`surface.c`). See [subsystems/world.md](subsystems/world.md) | `bg.c`, `bgfog.c`, `sky.c` |
| `model/` | Model files, skeletons, animation, joint lists and the model slots. `cobjdata.c`, `gobjdata.c` and `pobjdata.c` hold the tables of character, gun and prop model files. See [subsystems/models.md](subsystems/models.md) | `model.c`, `anim.c`, `jointlist.c`, `modelfile.c` |
| `gfx/` | Textures, the display list allocator, RSP task setup, z-buffer, per-instance vertex copies. See [subsystems/rendering.md](subsystems/rendering.md) | `texload.c`, `texdecode.c`, `texcmds.c`, `dyn.c`, `rsp.c`, `zbuf.c`, `vtxstore.c`, `texrect.c`, `imagebank.c` |
| `text/` | Strings and the language banks (`language.c`), fonts and drawing text (`text.c`). See [subsystems/text.md](subsystems/text.md) | `language.c`, `text.c` |
| `math/` | Matrices, quaternions, trig, interpolation, gravity. See [subsystems/math.md](subsystems/math.md) | `matrix.c`, `quaternion.c`, `trig.c` |
| `asset/` | Loading files from ROM and decompressing them. See [subsystems/assets-memory.md](subsystems/assets-memory.md) | `asset.c`, `rzip.c` |
| `audio/` | Which music plays in a level and when it changes, and the volume of sounds heard from somewhere in the level. See [subsystems/audio.md](subsystems/audio.md) | `levelmusic.c`, `sfxvolume.c` |
| `replay/` | Rare's demo playback, and the test hooks the replay tests use | `ramromreplay.c`, `testhooks.c` |

Each layer mostly calls the ones below it: menus and the level loop drive the player, guards
and props; those use navigation, models and the world; everything draws through `gfx/` and
uses `math/`.

## Small init and cleanup files

The original code was split into many tiny files named after what they set up or tear down
(`initguards.c`, `inititemslots.c`), each holding one or two
functions. They sit in the folder of the subsystem they belong to and will be merged into
that subsystem's main file.

## Misleading names

| File | What it actually holds |
| --- | --- |
| `model/initactorpropstuff.c` | Weapon animation groups and ejected casings |
| `player/bondview.c` | The camera helpers (`cam*`) and the player globals, not Bond's view as a whole |
| `player/bondview2.c` | Most of the player: movement, camera modes, the watch, health, the HUD, co-op and counter-op |
| `audio/levelmusic.c` | The level music state and the music slots (was `mp_music.c`), used in solo too |
| `audio/sfxvolume.c` | Volume of a sound from its distance to the nearest player (was in `propobj.c`) |
