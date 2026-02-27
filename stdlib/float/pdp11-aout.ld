OUTPUT_FORMAT("a.out-pdp11")
ENTRY(start)
STARTUP(crt0rt.o)
phys = 00001000;
SECTIONS
{
    .text phys : AT(phys) {
        code = .;
        *(.text)
        *(.rodata)
        *(.data)
        . = ALIGN(010);
    }
    end = .;
}
