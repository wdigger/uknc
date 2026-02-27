    .TITLE CRT0RT shim
    .IDENT "V00.00"

    STACK = 01000

    .text
    .GLOBAL _main

    .GLOBAL start
start:
        mov   $STACK, sp
        jsr   pc, _main
        emt   0350	// .EXIT
        nop
    .even

    .GLOBAL ___main
___main:
        rts   pc
    .even

    .end
