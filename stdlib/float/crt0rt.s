    .TITLE CRT0RT shim
    .IDENT "V00.00"

    .text
    .GLOBAL _main

    .GLOBAL start
start:
        mov   @$042, sp  /стек заполнится из заголовка, но на случай запуска не из Rt-11
        jsr   pc, _main
        emt   0350	// .EXIT
        nop
    .even

    .GLOBAL ___main
___main:
        rts   pc
    .even

    .end
