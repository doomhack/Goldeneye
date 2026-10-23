# assembler directives
.set noat      # allow manual use of $at
.set noreorder # don't insert nops after branches
.set gp=64

.include "macros.inc"

.section .text, "ax"

glabel boot
# Code used to run at 0x70000000 through a wired TLB entry; it now runs from KSEG0.
/* 0010AC 700004AC 3C0A7000 */  lui   $t2, %hi(init)
/* 0010B0 700004B0 254A0510 */  addiu $t2, $t2, %lo(init)
/* 0010B4 700004B4 01400008 */  jr    $t2
/* 0010B8 700004B8 00000000 */   nop

glabel get_csegmentSegmentStart
/* 0010BC 700004BC 3C028002 */  lui   $v0, %hi(_csegmentSegmentStart)
/* 0010C0 700004C0 03E00008 */  jr    $ra
/* 0010C4 700004C4 24420D90 */   addiu $v0, $v0, %lo(_csegmentSegmentStart)

glabel get_cdataSegmentRomStart
/* 0010C8 700004C8 3C020000 */  lui   $v0, %hi(_cdataSegmentRomStart)
/* 0010CC 700004CC 03E00008 */  jr    $ra
/* 0010D0 700004D0 24421990 */   addiu $v0, $v0, %lo(_cdataSegmentRomStart)

glabel get_cdataSegmentRomEnd
/* 0010D4 700004D4 3C020000 */  lui   $v0, %hi(_cdataSegmentRomEnd)
/* 0010D8 700004D8 03E00008 */  jr    $ra
/* 0010DC 700004DC 24423590 */   addiu $v0, $v0, %lo(_cdataSegmentRomEnd)

glabel get_inflateSegmentRomStart
/* 0010E0 700004E0 3C020000 */  lui   $v0, %hi(_inflateSegmentRomStart)
/* 0010E4 700004E4 03E00008 */  jr    $ra
/* 0010E8 700004E8 24423590 */   addiu $v0, $v0, %lo(_inflateSegmentRomStart)

glabel get_inflateSegmentRomEnd
/* 0010EC 700004EC 3C020000 */  lui   $v0, %hi(_inflateSegmentRomEnd)
/* 0010F0 700004F0 03E00008 */  jr    $ra
/* 0010F4 700004F4 24424B30 */   addiu $v0, $v0, %lo(_inflateSegmentRomEnd)

glabel jump_decompressfile
/* 0010F8 700004F8 3C077020 */  lui   $a3, %hi(decompress_entry)
/* 0010FC 700004FC 24E7141C */  addiu $a3, $a3, %lo(decompress_entry)
/* 001100 70000500 00E00008 */  jr    $a3
/* 001104 70000504 00000000 */   nop