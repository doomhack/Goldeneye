# F3DEX3

The graphics microcode for `make UCODE=f3dex3`: F3DEX3 by Sauraen, from
https://github.com/HackerN64/F3DEX3 at commit 91a85284821181af627d838e47d245718804f388.

Built with armips (Kingcom/armips 62adab4):

    make F3DEX3_BrZ_NOC F3DEX3_BrZ_NOC_PA F3DEX3_BrZ_NOC_PB

- `F3DEX3_BrZ_NOC`: the one the game uses (no occlusion plane).
- `_PA`, `_PB`: the same with F3DEX3's performance counters A and B, for profiling.

`include/PR/gbi_f3dex3.h` is F3DEX3's `gbi.h` from the same commit, with its two
includes pointed at `PR/`.

F3DEX3 Microcode - Sauraen
