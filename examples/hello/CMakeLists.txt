# The same program as the Makefile beside this file builds, for a
# project that would rather use CMake:
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=../../cmake/uknc-toolchain.cmake
#   cmake --build build
#
# hello.sav, hello.elf for gdb, hello.dsk to boot, and
# `cmake --build build --target run-hello` to run it on the machine.
cmake_minimum_required(VERSION 3.21)
project(hello C)

uknc_add_program(hello hello.c)
uknc_add_disk(disk NAME hello CONTENTS hello)
uknc_add_run(hello)
