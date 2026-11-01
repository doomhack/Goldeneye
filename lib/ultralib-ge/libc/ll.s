# ll.s: libultra's 64-bit integer helpers, written out from the original
# IDO -mips3 build. GCC can't emit 64-bit instructions under the o32 ABI.

.set noat
.set noreorder
.set mips3
.set gp=64
.set fp=64

.include "macros.inc"

.section .text, "ax"

glabel __ull_rshift
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    dsrlv $v0,$t6,$t7
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ull_rshift, . - __ull_rshift

glabel __ull_rem
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    ddivu $zero,$t6,$t7
    bnez $t7,.L__ull_rem_54
    nop
    break 0x7
.L__ull_rem_54:
    mfhi $v0
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ull_rem, . - __ull_rem

glabel __ull_div
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    ddivu $zero,$t6,$t7
    bnez $t7,.L__ull_div_90
    nop
    break 0x7
.L__ull_div_90:
    mflo $v0
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ull_div, . - __ull_div

glabel __ll_lshift
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    dsllv $v0,$t6,$t7
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ll_lshift, . - __ll_lshift

glabel __ll_rem
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    ddivu $zero,$t6,$t7
    bnez $t7,.L__ll_rem_f8
    nop
    break 0x7
.L__ll_rem_f8:
    mfhi $v0
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ll_rem, . - __ll_rem

glabel __ll_div
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    ddiv $zero,$t6,$t7
    nop
    bnez $t7,.L__ll_div_138
    nop
    break 0x7
.L__ll_div_138:
    daddiu $at,$zero,-1
    bne $t7,$at,.L__ll_div_154
    daddiu $at,$zero,1
    dsll32 $at,$at,0x1f
    bne $t6,$at,.L__ll_div_154
    nop
    break 0x6
.L__ll_div_154:
    mflo $v0
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ll_div, . - __ll_div

glabel __ll_mul
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    dmultu $t6,$t7
    mflo $v0
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ll_mul, . - __ll_mul

glabel __ull_divremi
    lh $t7,18($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t6,8($sp)
    move $t8,$t7
    move $t9,$t8
    ddivu $zero,$t6,$t9
    bnez $t9,.L__ull_divremi_1c0
    nop
    break 0x7
.L__ull_divremi_1c0:
    mflo $t0
    sd $t0,0($a0)
    lh $t2,18($sp)
    ld $t1,8($sp)
    move $t3,$t2
    move $t4,$t3
    ddivu $zero,$t1,$t4
    bnez $t4,.L__ull_divremi_1e8
    nop
    break 0x7
.L__ull_divremi_1e8:
    mfhi $t5
    sd $t5,0($a1)
    jr $ra
    nop
.size __ull_divremi, . - __ull_divremi

glabel __ll_mod
    addiu $sp,$sp,-8
    sw $a0,8($sp)
    sw $a1,12($sp)
    sw $a2,16($sp)
    sw $a3,20($sp)
    ld $t7,16($sp)
    ld $t6,8($sp)
    ddiv $zero,$t6,$t7
    nop
    bnez $t7,.L__ll_mod_228
    nop
    break 0x7
.L__ll_mod_228:
    daddiu $at,$zero,-1
    bne $t7,$at,.L__ll_mod_244
    daddiu $at,$zero,1
    dsll32 $at,$at,0x1f
    bne $t6,$at,.L__ll_mod_244
    nop
    break 0x6
.L__ll_mod_244:
    mfhi $t8
    sd $t8,0($sp)
    bgez $t8,.L__ll_mod_25c
    nop
    bgtz $t7,.L__ll_mod_274
    nop
.L__ll_mod_25c:
    ld $t9,0($sp)
    blez $t9,.L__ll_mod_284
    nop
    ld $t0,16($sp)
    bgez $t0,.L__ll_mod_284
    nop
.L__ll_mod_274:
    ld $t1,0($sp)
    ld $t2,16($sp)
    daddu $t3,$t1,$t2
    sd $t3,0($sp)
.L__ll_mod_284:
    lw $v0,0($sp)
    lw $v1,4($sp)
    jr $ra
    addiu $sp,$sp,8
.size __ll_mod, . - __ll_mod

glabel __ll_rshift
    sw $a0,0($sp)
    sw $a1,4($sp)
    sw $a2,8($sp)
    sw $a3,12($sp)
    ld $t7,8($sp)
    ld $t6,0($sp)
    dsrav $v0,$t6,$t7
    dsll32 $v1,$v0,0x0
    dsra32 $v1,$v1,0x0
    jr $ra
    dsra32 $v0,$v0,0x0
.size __ll_rshift, . - __ll_rshift
