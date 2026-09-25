# uknc-toolchain.cmake -- build for the УКНЦ with CMake.
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/uknc/cmake/uknc-toolchain.cmake
#   cmake --build build
#
# What this gives a project: the cross compiler and binutils, the flags
# every program on this machine wants, `.sav` as what an executable is,
# and the paths to everything else the toolchain carries (the PPU
# linker, rt11dsk, the emulator, the system disk image, the firmware) as
# ordinary variables.  The commands that turn those paths into build
# rules -- uknc_add_program, uknc_add_ppu_module, uknc_add_disk -- come
# with it, from UKNC.cmake beside this file, which is included at the
# end.
#
# Nothing here is specific to this repository: the same file works from
# an unpacked release, which is laid out the same way.
#
# CMake reads a toolchain file again for every try_compile, so what is
# here has to stay cheap and say the same thing every time.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR pdp11)

# For projects that build for this machine among others: `if(UKNC)`.
set(UKNC 1)

# TARGET_OBJECTS on an executable -- how the ELF twin is linked from the
# very objects the .sav was linked from, rather than from a second
# compilation of the same sources -- is only allowed from 3.21.
if(CMAKE_VERSION VERSION_LESS 3.21)
  message(FATAL_ERROR "the УКНЦ toolchain file needs CMake 3.21 or newer")
endif()

# --------------------------------------------------------------------
# Where the toolchain is

# Three answers, in the order they are worth having: what the user said,
# the tree this file is itself a part of, and what is on PATH.  The
# middle one is why a project that names this file by its path needs to
# say nothing else.
if(NOT UKNC_ROOT AND DEFINED ENV{UKNC_ROOT})
  set(UKNC_ROOT "$ENV{UKNC_ROOT}")
endif()

if(NOT UKNC_ROOT)
  get_filename_component(_uknc_above "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  if(EXISTS "${_uknc_above}/gcc/bin/pdp11-uknc-rt11-gcc"
      OR EXISTS "${_uknc_above}/gcc/bin/pdp11-uknc-rt11-gcc.exe")
    set(UKNC_ROOT "${_uknc_above}")
  endif()
endif()

if(NOT UKNC_ROOT)
  # .../gcc/bin/pdp11-uknc-rt11-gcc, so the root is three above it.
  find_program(_uknc_gcc_on_path pdp11-uknc-rt11-gcc)
  if(_uknc_gcc_on_path)
    get_filename_component(_uknc_above "${_uknc_gcc_on_path}/../../.." ABSOLUTE)
    set(UKNC_ROOT "${_uknc_above}")
  endif()
endif()

if(NOT UKNC_ROOT)
  message(FATAL_ERROR
    "UKNC toolchain not found.  Point at it with -DUKNC_ROOT=<dir>, or put "
    "its gcc/bin on PATH.  <dir> is the directory that has gcc/, debugger/, "
    "rom/ and resources/ in it.")
endif()

get_filename_component(UKNC_ROOT "${UKNC_ROOT}" ABSOLUTE)
set(UKNC_ROOT "${UKNC_ROOT}" CACHE PATH "Root of the УКНЦ toolchain")

set(UKNC_BIN "${UKNC_ROOT}/gcc/bin")
set(UKNC_SYSROOT "${UKNC_ROOT}/gcc/xgcc/pdp11-uknc-rt11")

# find_program remembers its answer in the cache, which is what makes a
# project's own -DUKNC_RT11DSK=... stick.  It also makes a stale answer
# stick when UKNC_ROOT changes under an existing build directory, so an
# answer from outside the toolchain being used now is dropped first.
macro(_uknc_find var)
  if(${var})
    string(FIND "${${var}}" "${UKNC_ROOT}/" _uknc_inside)
    if(NOT _uknc_inside EQUAL 0)
      unset(${var} CACHE)
    endif()
  endif()
  find_program(${var} NAMES ${ARGN} NO_DEFAULT_PATH
    HINTS "${UKNC_BIN}" "${UKNC_ROOT}/gcc/dejagnu" "${UKNC_ROOT}/debugger/bin")
endmacro()

_uknc_find(UKNC_GCC pdp11-uknc-rt11-gcc)
if(NOT UKNC_GCC)
  message(FATAL_ERROR "no pdp11-uknc-rt11-gcc under ${UKNC_BIN}")
endif()

_uknc_find(UKNC_AR pdp11-uknc-rt11-gcc-ar pdp11-uknc-rt11-ar)
_uknc_find(UKNC_RANLIB pdp11-uknc-rt11-gcc-ranlib pdp11-uknc-rt11-ranlib)
_uknc_find(UKNC_NM pdp11-uknc-rt11-gcc-nm pdp11-uknc-rt11-nm)
_uknc_find(UKNC_OBJCOPY pdp11-uknc-rt11-objcopy)
_uknc_find(UKNC_OBJDUMP pdp11-uknc-rt11-objdump)
_uknc_find(UKNC_SIZE pdp11-uknc-rt11-size)
_uknc_find(UKNC_STRIP pdp11-uknc-rt11-strip)

# The rest of what the toolchain carries.  A project is welcome to use
# these directly; the commands in UKNC.cmake are these plus the argument
# order each one wants.
_uknc_find(UKNC_GDB pdp11-uknc-rt11-gdb)
_uknc_find(UKNC_LD_PPU pdp11-uknc-rt11-ld-ppu)
_uknc_find(UKNC_RT11DSK rt11dsk)
_uknc_find(UKNC_EMULATOR ukncbtldebug)
_uknc_find(UKNC_RUN uknc-run)

set(UKNC_SYSTEM_DISK "${UKNC_ROOT}/resources/rt11os.dsk"
  CACHE FILEPATH "RT-11 disk image a program's own image is made from")
set(UKNC_FIRMWARE "${UKNC_ROOT}/rom/uknc_rom_autoboot.bin"
  CACHE FILEPATH "Firmware for the emulator; this one boots unasked")
# libppu's gdb script, which puts a PPU module's symbols over the code
# running on the PPU.  See libs/libppu/ppu.gdb.
set(UKNC_PPU_GDB_SCRIPT "${UKNC_SYSROOT}/lib/ppu.gdb"
  CACHE FILEPATH "gdb script that gives the PPU side its symbols")

# --------------------------------------------------------------------
# The tools

set(CMAKE_C_COMPILER "${UKNC_GCC}")
set(CMAKE_ASM_COMPILER "${UKNC_GCC}")
set(CMAKE_AR "${UKNC_AR}")
set(CMAKE_RANLIB "${UKNC_RANLIB}")
set(CMAKE_NM "${UKNC_NM}")
set(CMAKE_OBJCOPY "${UKNC_OBJCOPY}")
set(CMAKE_OBJDUMP "${UKNC_OBJDUMP}")
set(CMAKE_STRIP "${UKNC_STRIP}")

# Only the compiler is looked for on the host; everything else is the
# machine's, and comes out of the sysroot rather than off this computer.
set(CMAKE_FIND_ROOT_PATH "${UKNC_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# --------------------------------------------------------------------
# The flags

# The same ones every example in this repository builds with.
#
# -ffunction-sections/-fdata-sections put every function and every
# variable in a section of its own, so that --gc-sections below can drop
# the ones nothing reaches.  On a machine with 64 KB of address space
# that is not a micro-optimisation: it is what keeps a program that uses
# a corner of a library from carrying the rest of it.
set(CMAKE_C_FLAGS_INIT "-fomit-frame-pointer -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

# What each build type means here.  Cache entries rather than the _INIT
# variables, which GCC's own module appends to rather than replaces --
# setting those would leave "-O2 -g -O2 -g -DNDEBUG" and no way to tell
# which half won.  -D on the command line still decides: those entries
# are in the cache before this file is read.
#
# Two departures from CMake's usual meanings, both for a machine with 64
# KB of address space and a debugger on the other end of a serial line:
# Debug is -Og and not -O0, which on this target costs a great deal of
# memory for stepping that is barely easier; and RelWithDebInfo does not
# define NDEBUG, so that assert() stays -- it is -O2 -g, exactly what
# every example in this repository is built with, and the default below.
set(CMAKE_C_FLAGS_DEBUG "-Og -g" CACHE STRING "C flags for Debug")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g"
  CACHE STRING "C flags for RelWithDebInfo")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -g -DNDEBUG"
  CACHE STRING "C flags for MinSizeRel")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "C flags for Release")

# -O2 with debug information: what every example here builds with, and
# what makes the .elf twin worth having.  Only a default -- -D on the
# command line still decides.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
    "Build type: Debug, Release, RelWithDebInfo, MinSizeRel")
endif()

# gnu23, as everything in this repository is written in.  A project that
# wants another standard sets C_STANDARD on its targets as usual.
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)

# An executable is a .sav: a flat RT-11 memory image, which is what this
# machine loads and runs.  The ELF with the same code in it, which is
# what gdb reads, is a separate output -- see uknc_add_program.
set(CMAKE_EXECUTABLE_SUFFIX ".sav")
set(CMAKE_EXECUTABLE_SUFFIX_C ".sav")
set(CMAKE_EXECUTABLE_SUFFIX_ASM ".sav")

# --------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/UKNC.cmake")
