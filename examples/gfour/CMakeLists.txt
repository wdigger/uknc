# The same program as the Makefile beside this file builds, for a
# project that would rather use CMake:
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=../../cmake/uknc-toolchain.cmake
#   cmake --build build
#
# Both sides of the machine: gfour.sav runs on the CPU, GFPPU.PPU is
# loaded onto the PPU by it at run time -- which is why the module has
# to be on the disk, while the program itself need not be (gdb puts that
# one into memory).  Each also gets an .elf twin with the symbols and
# DWARF for debugging; ppu.gdb puts the module's own over the code
# running on the PPU.
#
# GFPPU, and not gfourppu: an RT-11 file name is six characters and a
# three-character type, and the name on the disk is what
# ppuc_load_code() is given in gfour.c.
cmake_minimum_required(VERSION 3.21)
project(gfour C)

uknc_add_program(gfour gfour.c)
target_link_libraries(gfour PRIVATE ppu pdp11)

uknc_add_ppu_module(gfourppu gfourppu.c NAME gfppu LIBRARIES pdp11)

uknc_add_disk(disk NAME gfour CONTENTS gfour gfourppu)
