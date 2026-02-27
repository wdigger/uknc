.text
.globl ___adddf3, ___mulsf3, ___negsf2, ___extendsfdf2, ___fixdfsi
.globl ___addsf3, ___subsf3, ___divsf3, ___subdf3, ___truncdfsf2
.globl ___divsi3, ___modsi3, ___fixsfsi, ___floatsidf, ___floatunsisf
.globl ___ltsf2

___addsf3:
    mov r0, -(sp)

    sub $8, sp
    mov 14(sp), (sp)
    mov 16(sp), 2(sp)
    mov 18(sp), 4(sp)
    mov 20(sp), 6(sp)

    mov sp, r0
    fadd r0

    mov 12(sp), r0
    mov 4(sp), (r0)
    mov 6(sp), 2(r0)

    add $8, sp
    mov (sp)+, r0
    rts pc

___adddf3:
    jmp ___addsf3


___subsf3:
    bit $0100000, 8(sp)
    beq 1f
    bic $0100000, 8(sp)
    br 2f
1:
    bis $0100000, 8(sp)
2:
    jmp ___addsf3

___subdf3:
    jmp ___subsf3


___mulsf3:
    mov r0, -(sp)
    sub $8, sp
    mov 14(sp), (sp)
    mov 16(sp), 2(sp)
    mov 18(sp), 4(sp)
    mov 20(sp), 6(sp)
    mov sp, r0
    fmul r0
    mov 12(sp), r0
    mov 4(sp), (r0)
    mov 6(sp), 2(r0)
    add $8, sp
    mov (sp)+, r0
    rts pc


___divsf3:
    mov r0, -(sp)
    mov r1, -(sp)
    sub $8, sp

    mov 16(sp), (sp)
    mov 18(sp), 2(sp)
    mov 20(sp), 4(sp)
    mov 22(sp), 6(sp)

    mov sp, r0
    clr r1
    fdiv r0

    mov 14(sp), r1
    mov (sp), (r1)
    mov 2(sp), 2(r1)

    add $8, sp
    mov (sp)+, r1
    mov (sp)+, r0
    rts pc


___negsf2:
    mov 2(sp), r0
    mov 4(sp), (r0)
    mov 6(sp), 2(r0)

    bit $0100000, (r0)
    beq 1f
    bic $0100000, (r0)
    br 2f
1:
    bis $0100000, (r0)
2:
    rts pc


/ Приведения типов типа
___extendsfdf2: rts pc
___fixdfsi:     jmp ___fixsfsi
___truncdfsf2: rts pc




___divsi3:
    mov r2, -(sp)
    mov r3, -(sp)

    mov 6(sp), r0
    mov 8(sp), r1
    mov 10(sp), r2

    mov 12(sp), r2

    tst r2
    beq err_div

    div r2, r0

    mov r0, r1
    clr r0
    br done_div

err_div:
    clr r0
    clr r1

done_div:
    mov (sp)+, r3
    mov (sp)+, r2
    rts pc


___modsi3:
    mov r2, -(sp)
    mov r3, -(sp)

    mov 6(sp), r0
    mov 8(sp), r1
    mov 12(sp), r2

    tst r2
    beq err_mod

    div r2, r0

    br done_mod

err_mod:
    clr r0
    clr r1

done_mod:
    mov (sp)+, r3
    mov (sp)+, r2
    rts pc


/;___floatsisf в pdp11-floatsisf.c
___floatunsisf:
___floatsidf:
    jmp ___floatsisf


___fixsfsi:
    mov r2, -(sp)
    mov r3, -(sp)

    mov 6(sp), r0
    mov r0, r2

    ash $-7, r0
    bic $0177400, r0
    sub $128, r0
    mov r0, r3

    blt fix_zero

    mov 6(sp), r0
    bic $0177600, r0
    bis $0000200, r0

    ash $8, r0

    mov 8(sp), r1
    clc
    ash $-8, r1
    bic $0177400, r1
    bis r1, r0

    sub $16, r3
    mov r3, r1

    tst r1
    bge 2f

1:  clc
    ror r0
    inc r1
    blt 1b
    br 3f

2:  ash r1, r0
3:
    tst r2
    bpl 4f
    neg r0
4:
    mov r0, r1
    sxt r0
    br fix_exit

fix_zero:
    clr r0
    clr r1
fix_exit:
    mov (sp)+, r3
    mov (sp)+, r2
    rts pc


___ltsf2:
    mov 2(sp), r0
    mov 6(sp), r1
    cmp r0, r1
    blt 1f
    cmp 4(sp), 10(sp)
1:  rts pc

.end
