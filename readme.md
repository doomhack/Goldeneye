# GoldenEye 007: co-op edition

A fork of the [GoldenEye 007 decompilation](https://github.com/n64decomp/007) that turns the
N64 game into a couch co-op game. Two players can play the whole solo campaign together, or
one plays Bond while the other takes over the guards. It also adds a hi-res mode, a faster
graphics microcode and a handful of multiplayer extras.

It still runs on a stock Nintendo 64 with an **Expansion Pak**, from a flash cart, on an
Analogue 3D, or in an accurate emulator such as [ares](https://ares-emu.net/). It builds the
NTSC (US) and PAL (EU) versions.

This repository holds no game data. You need your own copy of the original ROM to build
it: see [Building from source](docs/Building.md).

* [What's new for players](#whats-new-for-players)
* [What you need to play](#what-you-need-to-play)
* [What changed in the code](#what-changed-in-the-code)
* [Building](#building)
* [Documentation](#documentation)
* [Credits](#credits)

## What's new for players

### 2 player co-op

Pick **2 PLAYER CO-OP** on the mode select screen, then choose a mission and difficulty as
usual. Both players play the solo mission together, in split screen.

* **Every solo mission** can be finished in co-op, on every difficulty, and either player can
  complete any objective. Guards, drone guns and cameras fight both of you.
* **Shared progress.** Objectives, keys, keycards and mission items are shared. A one-use
  item such as the Dam modem or the Frigate tracker only fails the mission when neither
  player can still use one.
* **No friendly fire.** Killing a civilian still fails the mission, whoever does it.
* **Respawning, Perfect Dark style.** When one player dies the mission carries on. The dead
  player can press Start to come back at the level start, taking half of the living
  player's health (armour counts as health): 100% health and 50% armour becomes 75% each.
  You can't respawn if your partner is below 20%, and you can stay dead and spectate if you'd
  rather. The mission fails only when both players are dead.
* **Your own Bond.** Player 2 is a second Bond in a different outfit by default, or any
  multiplayer character. Each player has their own controls, control style, aim and sight
  settings and watch options.
* **Split screen your way.** Stacked (top and bottom, the default) or side by side, with or
  without hi-res. The views fade through black to a single full-screen view for the intro,
  cutscenes and the watch, and back again.
* **Pause together.** Either player's Start pauses both, and the player who paused gets
  their own watch.
* **Cheats** all work in co-op, for both players. Unlocks and saves are the same as solo.
* **The tank** belongs to whoever climbs in first. It can run the other player over, and if
  its driver dies it stays where it was for the respawned player to use.
* Both players appear in the intro cutscene, and both players' stats are shown at the end.
* The draw distance is tuned for split screen on the open levels (Jungle, Depot, Streets,
  Control, Bunker 2, Aztec, Egyptian) so co-op stays playable.

### 2 player counter-op

Pick **2 PLAYER COUNTER-OP**. One player is Bond and plays the mission as normal. The other
player is one of the enemy guards and has one job: stop Bond.

* The guard player takes over a real armed guard, somewhere out of Bond's sight and at least
  20 m away. When that guard dies, they take over another one, so they are never out of the
  game, and every level is playable.
* The guard player can't pick up weapons or ammo, or use consoles and mission items, but
  they *can* set off alarms.
* Instead of an empty hand, the guard player carries a **suicide pill**: select it and fire
  to give up the current guard and jump to another one.
* The guards don't hear the guard player's shots, and the guard player's near misses don't
  give Bond away, so Bond can still sneak about.
* Power cheats (invincibility, all guns, infinite ammo and so on) only help Bond. World
  cheats (enemy rockets, DK mode, paintball, fast or slow animation) affect everyone.
  Invisibility hides Bond from the guard player too.
* The end screen names the winner and shows how many bodies the guard player went through
  and how much damage they did to Bond.
* The co-op setup page picks which controller plays Bond.

### Multiplayer

* **Side by side** split screen for two players, as well as the original stacked layout,
  and **hi-res** for 2 to 4 players.
* **Fire and Gold**, a new weapon set: ZMG, proximity mines, grenade launcher and golden gun.
* **Custom weapon sets.** A weapons page lists every set, and a Custom set lets you pick the
  weapon in each of the 8 slots. It is saved on the cartridge.
* **Dual wield**, an option on the multiplayer options page. Pick up a second copy of any
  weapon that can be dual wielded to hold one in each hand.
* Player 2's co-op controls and options carry over to deathmatch.

### Display and controls

* **Hi-res mode**: 640x240 on NTSC and 640x272 on PAL, twice the original width. Turn it
  on in the watch options or on the co-op setup page. Off by default.
* **Faster graphics** with the [F3DEX3](https://github.com/HackerN64/F3DEX3) microcode. Co-op
  split screen spends about a third less time on the RSP than with the original microcode.
* **Fewer hitches**: rooms, animations and weapon models stay in memory once loaded instead
  of being read from the cartridge again and again, which uses the extra memory of the
  Expansion Pak.
* **Double-tap Start** to see your health and armour without pausing, in solo, co-op and
  counter-op. A single tap still opens the watch.
* **FPS counter**, an option on the watch, which also shows the CPU, RSP and RDP time per
  frame.
* Split screen messages, the ammo counter, gauges, timers and dialogue are laid out for half
  width views.

### Bug fixes

* PAL fixes that Rare made after the US release are in the US build too, including the
  Jungle "escort Natalya" script fix.
* PAL uses the same room limits as the US version, which removes PAL-only drawing glitches.
* Crashes from the original game that split screen exposed are fixed, such as the weapon
  pickup message in side by side views and far-away bullet tracers.
* If the game ever crashes, a crash screen shows the registers instead of a frozen picture,
  on real hardware too.

## What you need to play

* An **N64 Expansion Pak** (8 MB). Without one the game shows an "Expansion Pak required"
  screen.
* Your own build of the ROM: see [Building from source](docs/Building.md). It is the US or
  PAL version; there is no Japanese build.
* One of:
  * a Nintendo 64 with a flash cart (EverDrive and the like);
  * an Analogue 3D;
  * an emulator that runs the RSP at low level, such as [ares](https://ares-emu.net/).
    Emulators that use high-level RSP emulation can't run the F3DEX3 microcode and show a
    black or broken picture.

Saves use the cartridge's 4 Kbit EEPROM, as the original does.

## What changed in the code

The decomp builds a byte-for-byte copy of the original ROM with the original IDO compiler.
This fork gave that up so that the code could be changed freely.

* **Modern GCC.** Everything builds with GCC 15 (`-std=gnu23`, `-O2`, small data, link-time
  optimisation). The IDO compiler, the matching scripts, the Japanese and debug builds and the
  hidden ZX Spectrum emulator are gone.
* **8 MB memory layout.** The original game pages its code in from the cartridge through the
  TLB in 8 KB blocks to fit in 4 MB. Now all code is loaded once at boot and the heap runs to
  the top of 8 MB. The frame buffers and z-buffer are sized for hi-res.
* **ultralib.** Nintendo's library is the [ultralib](https://github.com/decompals/ultralib)
  decompilation (libultra 2.0L), unchanged, in `lib/ultralib`. Only a few files GoldenEye
  needs its own versions of are overridden, in `lib/ultralib-ge` (Rare's audio envelope code
  among them).
* **F3DEX3.** The graphics microcode is Sauraen's [F3DEX3](https://github.com/HackerN64/F3DEX3)
  (no occlusion plane variant) instead of Rare's Fast3D-based one. The level geometry and the
  model display lists are converted to F3DEX3 at build time, and the sky was rewritten to draw
  through the RSP. See [rsp/f3dex3](rsp/f3dex3/README.md).
* **Readable code.** The decomp's code was rewritten into clean C, a folder per subsystem in
  `src/game`, with readable names for functions, structures and fields, and the remaining
  assembly turned into C. Each subsystem has a page in [docs/subsystems](docs/subsystems).
  See [SourceLayout.md](docs/SourceLayout.md) and [StyleGuide.md](docs/StyleGuide.md).
* **Game modes.** `g_GameMode` (solo, co-op, counter-op, multiplayer) replaced the old
  "player count" checks, so code says which mode it means. Per-player state that used to be
  global (camera, tank, options, mission state) now belongs to each player.
* **Performance.** Hot structures were slimmed to fit the data cache, cache lines that were
  read once and evicted were cut, repeated full scans in the room, portal and prop code were
  removed, line of sight tests exit early, and AI script lookups are cached.
* **Fixed hardware-only bugs.** Emulators finish DMA instantly and don't model the data
  cache, so some bugs only crash a real console. The main one: a ROM read into a buffer that
  shared a cache line with other data. ROM reads now go through an aligned bounce buffer.
* **GCC versus IDO.** Code that relied on IDO quirks was fixed: GCC makes enums without
  negative values unsigned, many stack arrays were too small and only IDO's layout hid it, and
  a few out-of-bounds reads only worked by luck.
* **Testing tools.** Since the ROM no longer matches, these catch accidental changes: Rare's
  demo replays as a regression test, ROM and per-function comparisons, host-side unit tests,
  a data cache model and a headless emulator runner. See [Testing.md](docs/Testing.md).

## Building

In short, on Linux or WSL, with the GCC 15 toolchain on your `PATH` and your ROMs in the
repository root as `baserom.u.z64` (and `baserom.e.z64` for PAL):

```bash
make build_tools extractassets
make                    # US: build/u/ge007.u.z64
make VERSION=EU         # PAL: build/e/ge007.e.z64
```

[docs/Building.md](docs/Building.md) has the full steps: the toolchain, which ROMs work,
the build options and how to put the result on a flash cart or an emulator.

## Documentation

* [Building from source](docs/Building.md): the toolchain, the original ROMs, build options
  and troubleshooting.
* [Source layout](docs/SourceLayout.md): where things are, and the US and PAL build flags.
* [Subsystems](docs/subsystems): a page per part of the game (guards, player, rendering...).
* [Testing](docs/Testing.md): how to check a change didn't break anything.
* [Style guide](docs/StyleGuide.md): code conventions for contributions.

## Credits

* **The GoldenEye 007 decompilation**, [n64decomp/007](https://github.com/n64decomp/007),
  by its authors and contributors (the "00 Agents"). This fork is built on their work, and
  none of it would exist without it. GoldenEye and Perfect Dark documentation by Zoinkity.
* **ultralib**, [decompals/ultralib](https://github.com/decompals/ultralib): the decompiled
  libultra the game is built with.
* **F3DEX3 Microcode - Sauraen**, [HackerN64/F3DEX3](https://github.com/HackerN64/F3DEX3):
  the graphics microcode.
* **armips** by Kingcom, [Kingcom/armips](https://github.com/Kingcom/armips), which builds
  F3DEX3.
* The [Perfect Dark decompilation](https://gitlab.com/ryandwyer/perfect-dark), whose GCC
  build and co-op design were a guide for this one.
* GoldenEye 007 was made by Rare and published by Nintendo. This project is not affiliated
  with either, and you need your own copy of the game to use it.
