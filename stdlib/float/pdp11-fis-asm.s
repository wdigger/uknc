.text
.globl ___adddf3, ___mulsf3, ___negsf2, ___extendsfdf2, ___fixdfsi
.globl ___addsf3, ___subsf3, ___divsf3, ___subdf3, ___truncdfsf2
.globl ___divsi3, ___modsi3, ___fixsfsi, ___floatdisf, ___floatunsisf
.globl ___ltsf2, ___eqsf2, ___nesf2, ___gtsf2, ___lesf2, ___gesf2, ___cmpsf2
.globl ___fixunssfsi, ___udivsi3, ___umodsi3
.globl ___floatsisf

___addsf3:
    mov 06(sp), -(sp)
    mov 06(sp), -(sp)
    mov 016(sp), -(sp)
    mov 016(sp), -(sp)
    fadd sp
    mov (sp)+, @04(sp)
    add $2, 04(sp)
    mov  (sp)+, @02(sp)
    rts pc

___adddf3:
    jmp ___addsf3


___subsf3:
    mov 06(sp), -(sp)
    mov 06(sp), -(sp)
    mov 016(sp), -(sp)
    mov 016(sp), -(sp)
    fsub sp
    mov (sp)+, @04(sp)
    add $2, 04(sp)
    mov  (sp)+, @02(sp)
    rts pc


___subdf3:
    jmp ___subsf3


___mulsf3:
    mov 06(sp), -(sp)  // Младшая часть a
    mov 06(sp), -(sp)  // Старшая часть a
    mov 016(sp), -(sp) // Младшая часть b
    mov 016(sp), -(sp) // Старшая часть b
    fmul sp
    mov (sp)+, @04(sp) // Старшая часть результата
    add $2, 04(sp)
    mov  (sp)+, @02(sp) // Младшая часть результата
    rts pc


// 2(sp) - адрес результата
// 4(sp) - старшая часть делимого
// 6(sp) - младшая часть делимого
// 10(sp) - старшая часть делителя
// 12(sp) - младшая часть делителя
___divsf3:
    mov 06(sp), -(sp)  // Младшая часть делимого
    mov 06(sp), -(sp)  // Старшая часть делимого
    mov 016(sp), -(sp) // Младшая часть делителя
    mov 016(sp), -(sp) // Старшая часть делителя
    fdiv sp
    mov (sp)+, @04(sp) // Старшая часть результата
    add $2, 04(sp)
    mov  (sp)+, @02(sp) // Младшая часть результата
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



/___divsi3:
    mov r2, -(sp)
    
    mov 4(sp), r0
    mov 6(sp), r1
    mov 10(sp), r2

    tst r2
    beq err_div

    div r2, r0
    
    bvs err_div /делимое = 0

    mov r0, r1
    clr r0
    br done_div

err_div:
    clr r0
    clr r1

done_div:
    mov (sp)+, r2
    rts pc

/___modsi3:
    mov r2, -(sp)
    
    mov 4(sp), r0
    mov 6(sp), r1
    mov 10(sp), r2

    tst r2
    beq err_mod

    div r2, r0

    bvs err_mod /делимое = 0
    
    br done_mod

err_mod:
    clr r0
    clr r1

done_mod:
    mov (sp)+, r2
    rts pc



/ 2(sp) - старшая часть делимого  14 - после сохранения
/ 4(sp) - младшая часть делимого  16
/ 6(sp) - старшая часть делителя  18
/ 8(sp) - младшая часть делителя  20

___divsi3:
div:  clr  -(sp)    / Тип операции - деление
  br  divmod

___modsi3:
mod:  mov  $1, -(sp)  / Тип операции - остаток от деления
  br  divmod

___udivsi3:
udiv:  mov  $256, -(sp)  / Тип операции - беззнаковое деление
  br  divmod

___umodsi3:
umod:  mov  $257, -(sp)  / тип операции - беззнаковый остаток от деления

divmod:  mov  r2, -(sp)  / Сохранить регистры
  mov  r3, -(sp)
  mov  r4, -(sp)
  mov  r5, -(sp)
  mov  $32, -(sp)  / Счётчик сдвигов

  mov  16(sp), r1  / R0:R1 - делимое
  mov  14(sp), r0
  bpl  1f    / Делимое положительное
  tstb  11(sp)    / Операция беззнаковая ?
  bne  2f    / Да
  neg  r1    / Изменить знак делимого
  adc  r0
  neg  r0
  incb  1(sp)    / Признак изменения знака частного
  br  2f
1:  bne  2f    / Старшая часть делимого не равна нулю
  tst  r1    / Проверка младшей части делимого на ноль
  beq  9f    / Равна нулю - результат ноль
2:  mov  20(sp), r3  / R2:R3 - делитель
  mov  18(sp), r2
  bpl  3f    / Делитель положительный
  tstb  11(sp)    / Операция беззнаковая ?
  bne  4f    / Да
  neg  r3    / Изменить знак делителя
  adc  r2
  neg  r2
  incb  1(sp)    / Признак изменения знака частного
  br  4f
3:  bne  4f    / Старшая часть делителя не равна нулю
  tst  r3    / Проверка младшей части делителя на ноль
  beq  9f    / Равна нулю - деление на ноль, результат ноль
4:  clr  r4    / Инициализация R4:R5 - текущий остаток
  clr  r5
5:  asl  r1    / Цикл сдвига делимого и частного (по совместительству)
  rol  r0
  rol  r5    / Вдвигается в остаток
  rol  r4
  cmp  r4, r2    / Остаток у нас больше или равен делителю (старшая часть)
  blo  7f    / Нет
  bne  6f    / Да
  cmp  r5, r3    / Остаток у нас больше или равен делителю (младшая часть)
  blo  7f    / Нет
6:  sub  r3, r5    / Вычесть из текущего остатка делимое
  sbc  r4
  sub  r2, r4
  inc  r1    / Прибавить один к частному
7:  decb  (sp)    / Уменьшить счётчик сдвигов
  bne  5b    / Цикл не окончен
  tstb  10(sp)    / Операция деления ?
  beq  8f    / Да
  mov  r5, r1    / Перенести остаток в R0:R1
  mov  r4, r0
  tstb  11(sp)    / Операция беззнаковая ?
  bne  10f    / Да
  tst  16(sp)    / Надо менять знак остатка?
  bpl  10f    / Нет
  neg  r1    / Сменить знак остатка
  adc  r0
  neg  r0
  br  10f
8:  rorb  1(sp)    / Надо менять знак частного?
  bcc  10f    / Нет
  neg  r1    / Изменить знак частного
  adc  r0
  neg  r0
  br  10f
9:  clr  r0    / Результат ноль
  clr  r1
10:  tst  (sp)+    / Убрать из стека счётчик
  mov  (sp)+, r5  / Востановить регистры
  mov  (sp)+, r4
  mov  (sp)+, r3
  mov  (sp)+, r2
  tst  (sp)+    / Убрать тип операции
  rts  pc




/ 2(sp) - адрес float     8 после сохранения регистров
/ 4(sp) - старшая часть  10
/ 6(sp) - младшая часть  12
___floatsisf:
  mov  r0, -(sp)  / Сохранить регистры
  mov  r1, -(sp)
  mov  r2, -(sp)

  mov  $160, r2  / Начальное значение порядка
  mov  12(sp), r1  / Младшая часть
  mov  10(sp), r0  / Старшая часть
  bne  1f    / Старшая ненулевая
  tst  r1    / Младшая часть равна нулю?
  beq  5f    / Да - в итоге ноль
  br  2f    / Младшая не ноль, старшая ноль
1:  bpl  2f    / Число положительное
  neg  r1    / Сменим знак числа
  adc  r0
  neg  r0
  bmi  3f    / Знак остался - мантиссу двигать не надо
2:  dec  r2    / Уменьшим порядок
  asl  r1    / Сдвинем мантиссу влево
  rol  r0
  bpl  2b    / Пока старший бит в старшей части не стоит
3:  add  $0200, r1  / Прибавим 0200 для округления
  adc  r0
  bcc  4f    / Переноса не было
  ror  r0    / Сдвинем мантиссу вправо на 1 разряд
  ror  r1
  inc  r2    / Увеличим порядок
4:  bic  $0100000, r0  / Уберём скрытый разряд
  clrb  r1    / Сдвиг мантиссы вправо на 8 разрядов
  bisb  r0, r1    / Можно и ASHC #-8.,R0
  swab  r1
  clrb  r0
  swab  r0
  swab  r2    / Перенесём порядок в старший байт
  asl  10(sp)    / Выдвинем знак
  ror  r2    / Вдвинем знак к порядку
  bis  r2, r0    / Наложим порядок на мантиссу
5:  mov  8(sp), r2  / Адрес результата float
  mov  r0, (r2)+  / Сохранить результат
  mov  r1, (r2)+

  mov  (sp)+, r2  / Восстановить регистры
  mov  (sp)+, r1
  mov  (sp)+, r0
  rts  pc


___floatdisf:
    jmp ___floatsisf


/ 2(sp) - адрес float     8 после сохранения регистров
/ 4(sp) - старшая часть  10
/ 6(sp) - младшая часть  12
___floatunsisf:
  mov  r0, -(sp)  / Сохранить регистры
  mov  r1, -(sp)
  mov  r2, -(sp)

  mov  $160, r2  / Начальное значение порядка
  mov  12(sp), r1  / Младшая часть
  mov  10(sp), r0  / Старшая часть
  bne  1f    / Старшая ненулевая
  tst  r1    / Младшая часть равна нулю?
  beq  5f    / Да - в итоге ноль
  br  2f    / Младшая не ноль, старшая ноль
1:  bmi  3f    / Старший разряд - мантиссу двигать не надо
2:  dec  r2    / Уменьшим порядок
  asl  r1    / Сдвинем мантиссу влево
  rol  r0
  bpl  2b    / Пока старший бит в старшей части не стоит
3:  add  $0200, r1  / Прибавим 0200 для округления
  adc  r0
  bcc  4f    / Переноса не было
  ror  r0    / Сдвинем мантиссу вправо на 1 разряд
  ror  r1
  inc  r2    / Увеличим порядок
4:  bic  $0100000, r0  / Уберём скрытый разряд
  clrb  r1    / Сдвиг мантиссы вправо на 8 разрядов
  bisb  r0, r1    / Можно и ASHC #-8.,R0
  swab  r1
  clrb  r0
  swab  r0
  swab  r2    / Перенесём порядок в старший байт
  clc      / Положительный знак
  ror  r2    / Вдвинем знак к порядку
  bis  r2, r0    / Наложим порядок на мантиссу
5:  mov  8(sp), r2  / Адрес результата float
  mov  r0, (r2)+  / Сохранить результат
  mov  r1, (r2)+

  mov  (sp)+, r2  / Восстановить регистры
  mov  (sp)+, r1
  mov  (sp)+, r0
  rts  pc



___fixsfsi:
/2(sp) - старшая часть float 4 - после сохранения R2
/4(sp) - младшая часть float 6

  mov  r2, -(sp)
  clr  r0
  clr  r1
  mov  4(sp), r2
  asl  r2
  clrb  r2
  swab  r2
  beq  1f
  sub  $128, r2
  ble  1f
  sub  $24, r2
  cmp  r2, $32
  bge  1f
  bisb  4(sp), r0
  mov  6(sp), r1
  bis  $128, r0
  ashc  r2, r0
  tst  4(sp)
  bpl  1f
  neg  r1
  adc  r0
  neg  r0
1:  mov  (sp)+, r2
  rts  pc

___fixunssfsi:
/ 4(sp) - Старшая часть float
/ 6(sp) - Младшая часть float

    mov   r2, -(sp)
    clr   r0
    clr   r1    
    mov   4(sp), r2
    tst   r2
    bmi   1f
    asl   r2
    clrb  r2
    swab  r2
    beq   1f
    sub   $128, r2
    ble   1f
    sub   $24, r2
    cmp   r2, $8 
    bgt   1f
    bisb  4(sp), r0
    mov   6(sp), r1
    bis   $128, r0
    ashc  r2, r0
1:  mov   (sp)+, r2
    rts   pc


___ltsf2:
___gtsf2:
___eqsf2:
___nesf2:
___gesf2:
___lesf2:
    jmp ___cmpsf2

/___cmpsf2:
        cmp     2(sp), 6(sp)
        bcs     1f
        bhi     2f
        cmp     4(sp), 8(sp)
        bcs     1f
        bhi     2f
        clr     r0
        br     3f
1:     neg     6(sp)
2:     mov     6(sp), r0
3:     rts     pc

___cmpsf2:
    mov  r1, -(sp)
    mov  r2, -(sp)
    / 'a' NaN?
    mov 6(sp), r0
    bit $0100000, r0
    beq 1f
    bit $0077400, r0
    beq is_nan

1:  / 'b' NaN?
    mov 10(sp), r0
    bit $0100000, r0
    beq 2f
    bit $0077400, r0
    beq is_nan

2:
    mov 6(sp), r1  /Старшее a
    mov 10(sp), r2  /Старшее b

    /Знаки
    mov r1, r0
    xor r2, r0
    bpl same_signs

    tst r1
    bmi less
    br greater

same_signs:
    tst r1
    bmi both_neg

both_pos:
    cmp r1, r2
    blt less
    bgt greater
    cmp 8(sp), 12(sp) / Младшие слова
    blo less
    bhi greater
    br equal

both_neg:
    cmp r1, r2
    bgt less
    blt greater
    cmp 8(sp), 12(sp)
    bhi less
    blo greater
    br equal

is_nan:
    mov $1, r0
    br cmp_exit

less:
    mov $-1, r0
    br cmp_exit

greater:
    mov $1, r0
    br cmp_exit

equal:
    clr r0

cmp_exit:
    mov  (sp)+, r2
    mov  (sp)+, r1
    rts pc
