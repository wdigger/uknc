#!/usr/bin/env python3

"""Make uknc_rom_autoboot.bin out of uknc_rom.bin.

The stock firmware stops at a boot menu and waits:

      ЗАГРУЗКА

    1 - диск           (0÷3): 0
    2 - кассета ПЗУ    (1,2): 1
    3 - сеть
    ...

which costs a keypress for "1", another for ENTER, and the couple of
thousand emulated frames spent waiting for them.  The patched firmware
takes the first line without asking: disk, drive 0.

Python rather than sh because the patch is arithmetic on 16-bit words in
a binary file, which sh is a poor tool for.  Run it here; it writes
uknc_rom_autoboot.bin next to uknc_rom.bin.

--- What the patch does -----------------------------------------------

The menu lives at PPU address 0101004, which is offset 01004 in the ROM
file (window 0 maps the first 8K of ROM at 0100000).  In the original:

    101004  012700 000004  MOV  #4, R0        ; argument to the menu
    101010  004467 000166  JSR  R4, 101202    ; show it, read a key
    101014  102100                            ; inline argument: the menu
                                              ; descriptor at 0102100 --
                                              ; item count, then a pointer
                                              ; per line
    101016  001757         BEQ  100756        ; a digit: remember it and
                                              ; come round again
    101020  100452         BMI  101146        ; ENTER: go with what is
                                              ; selected, R2 holding it
    101022  060207         ADD  R2, PC        ; otherwise dispatch
    101024  000754         BR   100756        ;   0 - nothing
    101026  000406         BR   101044        ;   1 - disk
    101030  000424         BR   101102        ;   2 - ROM cartridge
    ...

So a digit key returns with Z set and the choice in R2, and ENTER returns
with N set; the ENTER path at 0101146 is what actually boots.  The patch
puts the choice in R2 by hand and jumps straight there:

    101004  012702 000002  MOV  #2, R2        ; 2 = the disk line
    101010  000456         BR   101146        ; what ENTER would have done

The drive number is already right: 0100756, which runs before this,
clears it on the stack, and 0 is drive 0.

--- And why one more word has to change ------------------------------

The firmware checksums itself at power-on.  Patched without more, it
stops at

      СТАРТОВЫЙ ТЕСТ

    - ошибка ПЗУ  1

The test is a plain 16-bit sum over each 8K block, so the sum of block 1
has to come out the same as before.  The patch subtracts 04011 from it,
and the same amount goes back into an unused word in the block's own
tail -- 116 words of zeros at 0117430 through 0117777 -- which nothing
reads.
"""

import os
import sys

ROM_SIZE = 32256          # What the emulator reads, and the file's size
ROM_BASE = 0o100000       # PPU address the first byte appears at
BLOCK_WORDS = 0o20000 // 2  # 8K block, the unit the self-test sums over

# (address, before, after)
PATCH = [
    (0o101004, 0o012700, 0o012702),   # MOV #4,R0     -> MOV #2,R2
    (0o101006, 0o000004, 0o000002),   #   the 4       ->   the 2
    (0o101010, 0o004467, 0o000456),   # JSR R4,101202 -> BR 101146
]

# An unused word in the same block, to put the checksum difference back.
BALANCE = 0o117776


def get_word(data, address):
    offset = address - ROM_BASE
    return data[offset] | (data[offset + 1] << 8)


def put_word(data, address, value):
    offset = address - ROM_BASE
    data[offset] = value & 0xff
    data[offset + 1] = (value >> 8) & 0xff


def block_sum(data, block):
    total = 0
    for offset in range(block * 0o20000, (block + 1) * 0o20000, 2):
        if offset + 1 >= len(data):
            break
        total += data[offset] | (data[offset + 1] << 8)
    return total & 0xffff


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    source = os.path.join(here, 'uknc_rom.bin')
    target = os.path.join(here, 'uknc_rom_autoboot.bin')

    with open(source, 'rb') as f:
        original = f.read()

    if len(original) != ROM_SIZE:
        sys.exit('%s is %d bytes, expected %d' % (source, len(original), ROM_SIZE))

    # Refuse to patch a firmware that does not look like the one this was
    # worked out against: writing these words blind would produce a ROM
    # that fails its own self-test at best.
    for address, before, _after in PATCH:
        found = get_word(original, address)
        if found != before:
            sys.exit('at %s expected %s, found %s -- not the firmware this '
                     'patch was made for' % (oct(address), oct(before), oct(found)))
    if get_word(original, BALANCE) != 0:
        sys.exit('the word at %s is not free' % oct(BALANCE))

    patched = bytearray(original)
    for address, _before, after in PATCH:
        put_word(patched, address, after)

    difference = (block_sum(patched, 0) - block_sum(original, 0)) & 0xffff
    put_word(patched, BALANCE, (-difference) & 0xffff)

    for block in range(4):
        if block_sum(patched, block) != block_sum(original, block):
            sys.exit('block %d checksum moved' % block)

    with open(target, 'wb') as f:
        f.write(bytes(patched))

    print('wrote %s' % target)
    print('balanced %s into %s' % (oct((-difference) & 0xffff), oct(BALANCE)))


if __name__ == '__main__':
    main()
