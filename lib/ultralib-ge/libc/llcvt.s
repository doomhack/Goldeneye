# llcvt.s: libultra's 64-bit integer <-> float conversions, written out from the original
# IDO -mips3 build. GCC can't emit 64-bit instructions under the o32 ABI.

.set noat
.set noreorder
.set mips3
.set gp=64
.set fp=64

.include "macros.inc"

.section .text, "ax"

glabel __d_to_ll
    trunc.l.d $f4,$f12
    dmfc1 $v0,$f4
    nop
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __d_to_ll, . - __d_to_ll

glabel __f_to_ll
    trunc.l.s $f4,$f12
    dmfc1 $v0,$f4
    nop
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __f_to_ll, . - __f_to_ll

glabel __d_to_ull
    cfc1 $t6,$31
    li $v0,1
    ctc1 $v0,$31
    nop
    cvt.l.d $f4,$f12
    cfc1 $v0,$31
    nop
    andi $at,$v0,0x4
    andi $v0,$v0,0x78
    beqz $v0,.L__d_to_ull_b4
    lui $at,0x43e0
    mtc1 $at,$f5
    mtc1 $zero,$f4
    li $v0,1
    sub.d $f4,$f12,$f4
    ctc1 $v0,$31
    nop
    cvt.l.d $f4,$f4
    cfc1 $v0,$31
    nop
    andi $at,$v0,0x4
    andi $v0,$v0,0x78
    bnez $v0,.L__d_to_ull_ac
    nop
    lui $t7,%hi(__ull_highbit)
    ld $t7,%lo(__ull_highbit)($t7)
    dmfc1 $v0,$f4
    b .L__d_to_ull_c4
    or $v0,$v0,$t7
.L__d_to_ull_ac:
    b .L__d_to_ull_c4
    li $v0,-1
.L__d_to_ull_b4:
    dmfc1 $v0,$f4
    nop
    bltz $v0,.L__d_to_ull_ac
    nop
.L__d_to_ull_c4:
    ctc1 $t6,$31
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __d_to_ull, . - __d_to_ull

glabel __f_to_ull
    cfc1 $t6,$31
    li $v0,1
    ctc1 $v0,$31
    nop
    cvt.l.s $f4,$f12
    cfc1 $v0,$31
    nop
    andi $at,$v0,0x4
    andi $v0,$v0,0x78
    beqz $v0,.L__f_to_ull_150
    lui $at,0x5f00
    mtc1 $at,$f4
    li $v0,1
    sub.s $f4,$f12,$f4
    ctc1 $v0,$31
    nop
    cvt.l.s $f4,$f4
    cfc1 $v0,$31
    nop
    andi $at,$v0,0x4
    andi $v0,$v0,0x78
    bnez $v0,.L__f_to_ull_148
    nop
    lui $t7,%hi(__ull_highbit)
    ld $t7,%lo(__ull_highbit)($t7)
    dmfc1 $v0,$f4
    b .L__f_to_ull_160
    or $v0,$v0,$t7
.L__f_to_ull_148:
    b .L__f_to_ull_160
    li $v0,-1
.L__f_to_ull_150:
    dmfc1 $v0,$f4
    nop
    bltz $v0,.L__f_to_ull_148
    nop
.L__f_to_ull_160:
    ctc1 $t6,$31
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __f_to_ull, . - __f_to_ull

glabel __ll_to_d
    sw $a0,0($sp)
    sw $a1,4($sp)
    ld $t6,0($sp)
    dmtc1 $t6,$f4
    jr $ra
    cvt.d.l $f0,$f4
.size __ll_to_d, . - __ll_to_d

glabel __ll_to_f
    sw $a0,0($sp)
    sw $a1,4($sp)
    ld $t6,0($sp)
    dmtc1 $t6,$f4
    jr $ra
    cvt.s.l $f0,$f4
.size __ll_to_f, . - __ll_to_f

glabel __ull_to_d
    sw $a0,0($sp)
    sw $a1,4($sp)
    ld $t6,0($sp)
    dmtc1 $t6,$f4
    bgez $t6,.L__ull_to_d_1d0
    cvt.d.l $f0,$f4
    lui $at,0x41f0
    mtc1 $at,$f7
    mtc1 $zero,$f6
    nop
    add.d $f0,$f0,$f6
.L__ull_to_d_1d0:
    jr $ra
    nop
.size __ull_to_d, . - __ull_to_d

glabel __ull_to_f
    sw $a0,0($sp)
    sw $a1,4($sp)
    ld $t6,0($sp)
    dmtc1 $t6,$f4
    bgez $t6,.L__ull_to_f_200
    cvt.s.l $f0,$f4
    lui $at,0x4f80
    mtc1 $at,$f6
    nop
    add.s $f0,$f0,$f6
.L__ull_to_f_200:
    jr $ra
    nop
.size __ull_to_f, . - __ull_to_f

.section .rodata
.balign 8
# 1 << 63, ORed back in after converting a value >= 2^63
__ull_highbit:
    .dword 0x8000000000000000
