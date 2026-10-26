# Source layout

The game code lives in `src/game`, one folder per subsystem. The engine and platform code
(boot, threads, scheduler, memory pools, audio, controllers, video) is in `src/`, and
Nintendo's SDK is in `src/libultra` and `src/libultrare`.

Include game headers by their path from `src/`, for example `#include "game/chr/chrai.h"`.

Each subsystem gets its own page in [docs/subsystems](subsystems) as it is cleaned up.

Many file names still come from the decomp, which named files after ROM addresses or a guess
at their contents, so a name doesn't always match what is inside. Those files are listed
under "Misleading names" below and get renamed as each subsystem is cleaned up.

## Folders, from the top of the frame down

| Folder | What it does | Main files |
| --- | --- | --- |
| `level/` | Level lifetime: stage load, the per-frame tick and render (`lv.c`), frame timing, one-time game init | `lv.c`, `frametiming.c`, `initgamedata.c` |
| `menu/` | Front end and in-game menus: title sequence, folder and mission select, the watch (pause) menu and per-player options, multiplayer setup, cheats | `front.c`, `options.c`, `mpmenu.c`, `title*.c`, `cheat.c` |
| `save/` | EEPROM save files and 007 mode settings | `file.c`, `file2.c`, `crc.c` |
| `player/` | Bond: movement, collision and camera (`bondview*.c`), inventory, guns and firing, ejected casings | `bondview.c`, `bondview2.c`, `bondinv.c`, `gun.c`, `gunfire.c`, `player.c` |
| `hud/` | On-screen overlays: radar, health and armour bars, the death blood effect, bullet sparks | `radar.c`, `glass2.c`, `blood_animation.c` |
| `chr/` | Guards and other characters: actions and animation state (`chraction.c`), the AI script interpreter (`chrai.c`) and the global AI scripts (`chraidata.c`) | `chr.c`, `chraction.c`, `chrai.c`, `chrprop.c` |
| `prop/` | Props: doors, weapons on the floor, vehicles, explosions, glass, alarms | `prop.c`, `propobj.c`, `explosion.c`, `glass.c` |
| `nav/` | Where things can stand and walk: floor tiles (`stan.c`) and the pad and waypoint graph guards path along | `stan.c`, `padhalllv.c` |
| `mission/` | Objectives and briefings | `objective.c`, `objective_status.c` |
| `world/` | The level geometry: rooms and portals (`bg.c`), fog, sky, light fixtures | `bg.c`, `bgfog.c`, `sky.c` |
| `model/` | Model files, skeletons, animation tables and the model slot manager. `cobjdata.c`, `gobjdata.c` and `pobjdata.c` hold the tables of character, gun and prop model files | `model.c`, `loadobjectmodel.c`, `objecthandler.c` |
| `gfx/` | Textures, the display list allocator, RSP task setup, z-buffer, vertex pools | `image.c`, `tex.c`, `dyn.c`, `rsp.c`, `viewport.c` |
| `text/` | Text rendering and language banks | `textrelated.c`, `language.c` |
| `math/` | Matrices, quaternions, trig, interpolation, gravity. See [subsystems/math.md](subsystems/math.md) | `matrix.c`, `quaternion.c`, `trig.c` |
| `asset/` | Loading files from ROM and decompressing them | `ob.c`, `zlib.c`, `decompress.c` |
| `audio/` | Music track choice per level and multiplayer music | `music_0D2720.c`, `mp_music.c` |
| `replay/` | Rare's demo playback, and the test hooks the replay tests use | `ramromreplay.c`, `testhooks.c` |
| `debug/` | Debug menu, free camera, speed graph, host link (Indy) code. To be deleted in Phase 2 | `debugmenu_handler.c`, `debug_camera.c` |

Each layer mostly calls the ones below it: menus and the level loop drive the player, guards
and props; those use navigation, models and the world; everything draws through `gfx/` and
uses `math/`.

## Small init and cleanup files

The original code was split into many tiny files named after what they set up or tear down
(`initguards.c`, `cleanup_objects.c`, `initunk_005520.c`), each holding one or two
functions. They sit in the folder of the subsystem they belong to and will be merged into
that subsystem's main file.

## Misleading names

| File | What it actually holds |
| --- | --- |
| `hud/glass2.c` | Health and armour bars, the watch background, bullet sparks |
| `hud/bondwalk2.c` | Drawing a textured rectangle on screen |
| `model/math_unk_05A9E0.c` | One model joint helper, `modelGetBendStretchScale` |
| `world/unk_092E50.c` | Sky cloud and mipmap display lists |
| `world/initunk_007290.c` | Resets three floats used by `unk_092E50.c` |
| `player/ejectedcartridges.c` | Loads the gun casing models and resets their sound slots |
| `audio/mp_music.c` | The mission state (playing, cutscene, ended) and the music slots, used in solo too |
| `debug/deb_loadallmodels.c` | Debug model loading, but also `init_obj_register_difficulty_vals`, which guard setup calls on every level |
| `level/game_debug.c` | `gameInit`, the game's one-time setup, not debug code |
