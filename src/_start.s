# The first code that runs: the boot code (IPL3) loads the first 1MB of ROM to 0x80000400 and
# jumps here. C cannot run yet, so this stays in assembly: it clears .bss, sets up $gp for
# small data and the stack pointer, then jumps to init() in init.c.
.set noat      # allow manual use of $at
.set noreorder # don't insert nops after branches

.section .text, "ax"
.include "macros.inc"

glabel _start
    lui   $t0, %hi(_bssSegmentStart)
    lui   $t1, %hi(_bssSegmentSize)
    addiu $t0, %lo(_bssSegmentStart)
    addiu $t1, %lo(_bssSegmentSize)
.Lclear:
    addi  $t1, $t1, -8
    sw    $zero, ($t0)
    sw    $zero, 4($t0)
    bnez  $t1, .Lclear
     addi $t0, $t0, 8

    lui   $gp, %hi(_gp)
    addiu $gp, %lo(_gp)
    lui   $t2, %hi(init)
    lui   $sp, %hi(sp_rmon)
    addiu $t2, %lo(init)
    jr    $t2
     addiu $sp, %lo(sp_rmon)
