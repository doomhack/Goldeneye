# ultralib (libultra 2.0L)

Nintendo's N64 library, libultra, from the ultralib decompilation:
https://github.com/decompals/ultralib, commit e24c836796df4bf520ff8b11a5c9d2cea3a66cbd (19 April 2026).
It is built as version 2.0L, the last release, for a cartridge (`_FINALROM`), with the game's GCC.

GoldenEye shipped with an older libultra (around 2.0D) that Rare had changed in a few places.
That copy was in `src/libultra` and `src/libultrare` until it was replaced by this one.

## What is here

Only the parts of the upstream repository the game can use: `include/` in full, and from `src/`
the `audio`, `gu`, `mgu`, `io`, `libc`, `os` and `vimodes` folders, plus
`debug/kdebugserver.c` (it holds `__osThreadSave`, which the exception handler needs). The
upstream build system, tools and the debug, host, 64DD voice and Game Boy Pak parts are left out.

Nothing in this folder is changed from upstream, so it can be updated by copying the same
folders over again.

## How it is built

`lib/ultralib.mk` compiles every file into one archive, `libultra.a`. The linker only takes
the objects the game calls, so unused files cost nothing.

GoldenEye's own versions of a few files are in `lib/ultralib-ge` and are built in place of the
upstream file with the same path:

| File | Why |
| --- | --- |
| `audio/env.c` | Rare's envelope mixer. Its volume ramps are linear where the stock one is exponential, and the game's sounds and music are mixed for that. |
| `os/createthread.c` | Sets `$gp` for each new thread. The game and library use small data (`-G 8`), reached through `$gp`. |
| `libc/ll.s`, `libc/llcvt.s` | 64-bit integer maths and conversions in assembly. The upstream C versions only work with the original compiler, which used 64-bit registers. |
| `nosdata.h` | Included first in each library C file: keeps variables that are not small data (the boot variables at fixed addresses, data defined in assembly) off `$gp`. |

## Types

The library's headers define `s32` and `u32` as `long`. The game keeps them as `int`
(`include/PR/ultratypes.h` is found first on the game's include path). Both are 32 bits, so the
two sides agree on every call. Game code includes `<ultra64.h>` (`include/ultra64.h`), which
pulls in the library headers and adds the game's few extras.
