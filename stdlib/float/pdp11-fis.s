.text
.globl ___adddf3, ___mulsf3, ___negsf2, ___extendsfdf2, ___fixdfsi
.globl ___addsf3, ___subsf3, ___divsf3, ___subdf3, ___truncdfsf2
.globl ___divsi3, ___modsi3, ___fixsfsi, ___floatdisf, ___floatunsisf
.globl ___ltsf2, ___eqsf2, ___nesf2, ___gtsf2, ___lesf2, ___gesf2, ___cmpsf2
.globl ___fixunssfsi, ___udivsi3, ___umodsi3, ___udivhi3, ___umodhi3
.globl ___floatsisf, ___muldf3, ___divdf3


//функции для обрезанного double
/ 2(sp) - адрес результата
/ 4(sp),6(sp),8(sp),10(sp) - первый аргумент
/ 12(sp),14(sp),16(sp),18(sp) - второй аргумент
___adddf3:
    mov 6(sp), -(sp)
    mov 6(sp), -(sp)
    mov 18(sp), -(sp)
    mov 18(sp), -(sp)
    fadd sp
    mov 6(sp), r0
    mov (sp)+, (r0)+
    mov (sp)+, (r0)+
    clr (r0)+
    clr (r0)+
    rts pc

___subdf3:
    mov 6(sp), -(sp)
    mov 6(sp), -(sp)
    mov 18(sp), -(sp)
    mov 18(sp), -(sp)
    fsub sp
    mov 6(sp), r0
    mov (sp)+, (r0)+
    mov (sp)+, (r0)+
    clr (r0)+
    clr (r0)+
    rts pc

___muldf3:
    mov 6(sp), -(sp)
    mov 6(sp), -(sp)
    mov 18(sp), -(sp)
    mov 18(sp), -(sp)
    fmul sp
    mov 6(sp), r0
    mov (sp)+, (r0)+
    mov (sp)+, (r0)+
    clr (r0)+
    clr (r0)+
    rts pc

___divdf3:
    mov 6(sp), -(sp)
    mov 6(sp), -(sp)
    mov 18(sp), -(sp)
    mov 18(sp), -(sp)
    fdiv sp
    mov 6(sp), r0
    mov (sp)+, (r0)+
    mov (sp)+, (r0)+
    clr (r0)+
    clr (r0)+
    rts pc

//


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
    mov 4(sp), r1
    mov $0100000, r0
    xor r0, r1    
    mov 2(sp), r0
    mov r1, (r0)
    mov 6(sp), 2(r0)    
    rts pc


/ Приведения типов типа
___fixdfsi:     jmp ___fixsfsi


___extendsfdf2:
    / Сохраняем R0 и R1, так как мы их используем
    mov r0, -(sp)
    mov r1, -(sp)

    / Стек теперь:
    / 00(sp) - сохраненный r1
    / 02(sp) - сохраненный r0
    / 04(sp) - адрес возврата
    / 06(sp) - УКАЗАТЕЛЬ на результат (тот r4 из вызова)
    / 010(sp) - первое слово float
    / 012(sp) - второе слово float

    mov 06(sp), r0    / Получаем адрес для записи результата
    mov 010(sp), (r0)+ / Копируем float слово 1
    mov 012(sp), (r0)+ / Копируем float слово 2
    clr (r0)+          / Double требует еще 2 слова
    clr (r0)+

    / Восстанавливаем регистры
    mov (sp)+, r1
    mov (sp)+, r0
    rts pc


___truncdfsf2:
    / Сохраняем r2, так как в _start там лежит адрес printf
    mov r2, -(sp)    

    / Теперь стек:
    / 00(sp) - сохраненный r2
    / 02(sp) - адрес возврата
    / 04(sp) - КУДА писать результат (указатель)
    / 06(sp) - слово 1 double
    / 010(sp) - слово 2 double
    / ... (еще 2 слова double)

    mov 04(sp), r2    / Берем адрес назначения
    mov 06(sp), (r2)+ / Копируем старшее слово double (как float)
    mov 010(sp), (r2) / Копируем второе слово double
    
    / Восстанавливаем r2
    mov (sp)+, r2
    rts pc            / ВОЗВРАТ. Стек очистит вызывающий (caller)




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


___udivhi3:
    mov   r2, -(sp)
    mov   r3, -(sp)
    
    mov   6(sp), r0
    mov   8(sp), r2
    beq   3f  

    clr   r1
    mov   $16, r3

1:  asl   r0
    rol   r1
    cmp   r1, r2
    blo   2f
    sub   r2, r1
    inc   r0
2:  sob   r3, 1b

    br    4f

3:  clr   r0              / Деление на 0
4:  mov   (sp)+, r3
    mov   (sp)+, r2
    rts   pc


___umodhi3:
    mov   r2, -(sp)
    mov   r3, -(sp)
    
    mov   6(sp), r0
    mov   8(sp), r2
    beq   3f

    clr   r1
    mov   $16, r3

1:  asl   r0
    rol   r1
    cmp   r1, r2
    blo   2f
    sub   r2, r1
    inc   r0
2:  sob   r3, 1b

    mov   r1, r0
    br    4f

3:  clr   r0
4:  mov   (sp)+, r3
    mov   (sp)+, r2
    rts   pc


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

___cmpsf2:
  cmp  2(sp), 6(sp)
  bcs  1f
  bhi  2f
  cmp  4(sp), 8(sp)
  bcs  1f
  bhi  2f
  clr  r0
  br  3f
1:  
  neg  6(sp)
  mov  6(sp), 2(sp)
2:  
  mov  2(sp), r0
3:  
  rts  pc


