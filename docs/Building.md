# Building from source

This page takes you from a fresh clone to a ROM you can play. The repository holds only code:
all the game's graphics, sound, levels and text are extracted from your own copy of the
original GoldenEye 007 ROM, so you need one before you start.

* [1. What you need](#1-what-you-need)
* [2. Get the original ROMs ready](#2-get-the-original-roms-ready)
* [3. Install the build tools](#3-install-the-build-tools)
* [4. Install the GCC 15 toolchain](#4-install-the-gcc-15-toolchain)
* [5. Extract the assets](#5-extract-the-assets)
* [6. Build the ROM](#6-build-the-rom)
* [7. Play it](#7-play-it)
* [Build options](#build-options)
* [Updating and rebuilding](#updating-and-rebuilding)
* [Docker](#docker)
* [Troubleshooting](#troubleshooting)
* [How the build works](#how-the-build-works)

## 1. What you need

* A Linux machine, or Windows with [WSL 2](https://learn.microsoft.com/windows/wsl/install)
  (Ubuntu 22.04 or newer), or Docker. macOS isn't tested.
* About 2 GB of free disk space, most of it for the toolchain and the extracted assets.
* The original **US (NTSC)** GoldenEye 007 ROM. To build the **PAL** version you also need the
  original **PAL (EU)** ROM, because PAL assets are extracted as a difference from the US ones.
  The Japanese version isn't supported.

## 2. Get the original ROMs ready

The build only accepts these exact dumps:

| File name | Version | SHA-1 |
| --- | --- | --- |
| `baserom.u.z64` | US (NTSC) | `abe01e4aeb033b6c0836819f549c791b26cfde83` |
| `baserom.e.z64` | PAL (EU) | `167c3c433dec1f1eb921736f7d53fac8cb45ee31` |

Copy them into the root of the repository (next to the `Makefile`) with those names, and check
them:

```bash
sha1sum baserom.u.z64 baserom.e.z64
```

If the hash doesn't match, your dump is probably in a different byte order. ROMs come in three
orders, and the build needs the big-endian `.z64` one:

* `.z64`: big-endian, the native order. Use as it is.
* `.v64`: byte-swapped. Convert with `dd if=rom.v64 of=baserom.u.z64 conv=swab`.
* `.n64`: little-endian. Convert with an N64 ROM tool such as Tool64, or any byte-order
  converter that outputs `.z64`.

Then check the hash again. Any other mismatch means a different revision or a modified ROM,
which won't work.

The ROM files are listed in `.gitignore`. Never commit them or share them.

## 3. Install the build tools

On Debian or Ubuntu (including WSL):

```bash
sudo apt-get update
sudo apt-get install build-essential make git python3 curl xz-utils
```

`build-essential` provides the host C compiler, which builds the small helper programs in
`tools/` (the asset extractor, the compressor and the checksum tool). armips, which builds
the F3DEX3 microcode, comes with the repository (`tools/armips.cpp`), or the build uses an
installed `armips` if it finds one.

## 4. Install the GCC 15 toolchain

The game is compiled with GCC 15 for `mips-linux-gnu`, with link-time optimisation (LTO). It
uses C23 (`-std=gnu23`), so the older `gcc-mips-linux-gnu` packages in Ubuntu 22.04 and 24.04
are too old.

Build it with the script in this repository. It downloads GCC 15.2 and binutils 2.45 from
ftp.gnu.org and takes about half an hour on 4 cores:

```bash
sudo apt-get install bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev
sudo mkdir -p /opt/ge-gcc15 && sudo chown "$USER" /opt/ge-gcc15
tools/toolchain/build-gcc.sh /opt/ge-gcc15
```

Then put it first on your `PATH`, for example in `~/.bashrc`:

```bash
export PATH=/opt/ge-gcc15/bin:$PATH
```

Check it:

```bash
mips-linux-gnu-gcc --version     # should say 15.2.0
```

The script builds only the compiler and binutils, with no C library: the game brings its own
helpers (`src/gcc/libgcc_compat.c`). The comments at the top of the script explain the
choices, such as why the target is `mips-linux-gnu` rather than `mips64-elf`.

If you already have a GCC 15 cross compiler without LTO support, the build still works:
the Makefile notices and falls back to `LTO=0`, which gives a slightly larger, slightly
slower ROM.

## 5. Extract the assets

This step builds the host tools, then unpacks the levels, models, textures, music and text
from the original ROMs into `assets/` and turns the model data into C source. It takes a few
minutes and only has to be done once.

```bash
make build_tools extractassets
```

If you only have the US ROM, extract just the US assets:

```bash
make build_tools extract_u convert_props convert_chrs convert_guns
```

## 6. Build the ROM

```bash
make -j4                 # US version
make VERSION=EU -j4      # PAL version
```

The ROMs are written to:

| Version | ROM | ELF (symbols and debug info) |
| --- | --- | --- |
| US | `build/u/ge007.u.z64` | `build/u/ge007.u.elf` |
| PAL | `build/e/ge007.e.z64` | `build/e/ge007.e.elf` |

The `.elf` is the linked program the ROM is made from, with every symbol and full DWARF debug
info (source lines, types, locals). Load it in a debugger next to the matching ROM, for example
gdb connected to ares's GDB server: `target remote localhost:9123` after `file build/u/ge007.u.elf`
(use a MIPS gdb such as `gdb-multiarch`). Keep the two together: an `.elf` only matches the ROM
it was built with.

A full build takes a few minutes. The two versions build into separate folders, so you can
build both without cleaning in between.

## 7. Play it

The game **needs an Expansion Pak** (8 MB of RAM). Without one it shows a message asking for
one.

* **N64 with a flash cart:** copy the `.z64` to the SD card. The ROM header is the original
  GoldenEye one, so carts pick the right save type (4 Kbit EEPROM) by themselves. If yours
  asks, choose EEPROM 4K.
* **Analogue 3D:** load it from the SD card like any other ROM.
* **Emulator:** use one that emulates the RSP at low level, such as
  [ares](https://ares-emu.net/). The game uses the F3DEX3 microcode, which high-level RSP
  emulation doesn't know, so emulators or plugins that rely on it show a black or broken
  picture. Turn on the emulator's Expansion Pak (ares has it on by default).

## Build options

Options are passed on the `make` command line. The first value listed is the default.

| Option | Values | What it does |
| --- | --- | --- |
| `VERSION` | `US`, `EU` | Which version to build. `EU` is PAL. |
| `UNLOCK_ALL` | `0`, `1` | `1` opens every mission and difficulty (007 included) in every save folder, and every cheat in the cheat menu. Handy for testing or for jumping straight into co-op on any level. |
| `LTO` | `1`, `0` | Link-time optimisation. `0` turns it off, which builds faster. |
| `DEBUG_INFO` | `1`, `0` | Debug info in the `.elf`. `0` leaves it out, which builds a little faster; the ROM is byte-identical either way. |
| `FOGTUNE` | `0`, `1` | A tuning build: player 1 holds L and uses the C buttons to change the level's draw distance (C-left/right) and the prop fade distance (C-up/down), with the values on screen. |

For example, a PAL build with everything unlocked:

```bash
make VERSION=EU UNLOCK_ALL=1 -j4
```

Changing `UNLOCK_ALL` rebuilds only the files it affects, so switching back and forth is quick.

## Updating and rebuilding

After `git pull`, a plain `make` rebuilds whatever changed, header changes included.

If a change touches how assets are extracted or converted, the build stops and tells you to
re-extract:

```bash
make forceextractassets
```

To throw away all build output and start again:

```bash
make clean
```

## Docker

The `Dockerfile` builds an image with the host tools and the GCC 15 toolchain (it runs
`tools/toolchain/build-gcc.sh`, so the first image build takes about half an hour). Your
clone is mounted into the container, so the ROMs and the build output stay on your machine.

```bash
docker build -t goldeneye .
docker run --rm -it -v "$(pwd)":/home/dev/project goldeneye
# inside the container:
make build_tools extractassets
make -j4
```

Or with Docker Compose: `docker compose run ge007 bash`.

## Troubleshooting

**"baserom.u.z64 not found" or a hash mismatch.** See [step 2](#2-get-the-original-roms-ready).
The file must be in the repository root, named exactly `baserom.u.z64`, in `.z64` byte order.

**"Model.c files are out of date: run make forceextractassets".** Your extracted assets are
from an older version of the converters. Run `make forceextractassets`, then `make`.

**Errors about `-std=gnu23` or an unknown option.** The compiler on your `PATH` is older than
GCC 15. Check `which mips-linux-gnu-gcc` and that `/opt/ge-gcc15/bin` comes first.

**"This GCC has no LTO support: building with LTO=0".** Not an error: your toolchain was built
without LTO, so the build goes on without it.

**Errors running `tools/n64cksum` or `tools/gzipsrc/gzip`.** These host tools were built on
another machine or operating system (for example in Docker and then in WSL). Delete them and
run `make build_tools` again.

**Git says "dubious ownership".** The repository folder belongs to another user, which is
common in Docker and WSL. Run the `git config --global --add safe.directory ...` command that
Git prints.

**The game shows "Expansion Pak required".** The console or emulator has only 4 MB. Insert an
Expansion Pak, or turn it on in the emulator.

**Black screen or garbage in an emulator.** The emulator is using high-level RSP emulation.
Switch to a low-level RSP plugin, or use ares.

## How the build works

You don't need this to build the game, but it helps when changing it.

**Tools.** `make build_tools` compiles the host programs in `tools/` with the host compiler:
the asset extractor, Rare's `1172` compressor, gzip, armips and `n64cksum`.

**Assets.** `scripts/extract_baserom.u.sh` cuts the US ROM into files using
`scripts/filelist.u.csv`, and `scripts/extract_diff.e.sh` then applies the PAL differences
from `scripts/filediff.e.csv`. The `convert_chrs`, `convert_guns` and `convert_props` steps
turn the character, gun and prop models into C (`Model.c` files, gitignored), with their
display lists written as GBI macros that compile to F3DEX3 commands. The room geometry is
stored as the original microcode's commands and converted to F3DEX3 at build time by
`tools/f3dex3_bg.py`.

**Code.** The game in `src/`, and Nintendo's library in `lib/ultralib` (with GoldenEye's
own versions of a few files in `lib/ultralib-ge`), are compiled with GCC at `-O2`. The
library goes into `libultra.a`, and the linker takes only the parts the game uses. Header
dependencies are tracked, so `make` rebuilds what a header change affects. With LTO, the
game code is optimised as a whole at link time.

**Data.** Each asset is compiled from C (or taken as a binary), linked on its own into an
ELF, dumped to a `.bin` and compressed into a `.rz` file. The level and model files are
bundled in `assets/obseg/ob_seg.s` and the music in `assets/music/music.s`.

**Microcode.** `rsp/f3dex3` holds the prebuilt F3DEX3 microcode used by the game, and the
`_PA` and `_PB` variants with performance counters, which the fps counter's extra pages
read. See [rsp/f3dex3/README.md](../rsp/f3dex3/README.md) for how to rebuild it.

**The ROM.** The linker script `ge007.ld` (with the files in `ld/`) places the boot code, the
game code, the compressed data and the 8 MB memory layout. All game code is loaded into RAM
once at boot. `objcopy` makes the binary and `tools/n64cksum` writes the checksum the console
checks. The map file `build/[ue]/ge007.*.map` lists where everything ended up.

For the source layout see [SourceLayout.md](SourceLayout.md), and for checking that a change
didn't break anything see [Testing.md](Testing.md).
