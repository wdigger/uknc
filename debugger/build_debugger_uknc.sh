#!/usr/bin/env sh
set -e

# Builds ukncbtl-debugger, the console debugger this project runs its
# programs in, from
#
#   https://github.com/wdigger/ukncbtl-debugger  branch gdbserver
#
# That branch is this project's own: on top of the upstream console
# debugger it reads ELF symbols and DWARF source lines out of the file
# the linker produces alongside the .sav, steps by source line, and
# serves gdb's remote protocol so that pdp11-uknc-rt11-gdb can drive the
# machine ("gdbserver" there, "target remote :2345" here).
#
# Unlike the toolchain next door there is nothing to patch: the branch is
# the source, so this clones it rather than fetching commits one at a
# time over a release tarball.
#
# Run it from this directory.  It leaves src/ with the clone and bin/
# with the binary and the machine's firmware beside it, all of which are
# ignored by git.

BUILDDIR="${PWD}"

# Preparing folders
cd ${BUILDDIR}
mkdir src
mkdir bin

# Clone
git clone --depth 1 --single-branch --branch gdbserver https://github.com/wdigger/ukncbtl-debugger.git ${BUILDDIR}/src

# Build.  The debugger is self-contained C++17 -- it links UKNCBTL's
# emubase directly and has no library dependencies at all -- so this is
# just its own Makefile, which puts the result in build/release.
cd ${BUILDDIR}/src
make release -j4

cp ${BUILDDIR}/src/build/release/ukncbtldebug ${BUILDDIR}/bin/

# The emulator wants uknc_rom.bin in the current directory at run time,
# and the DejaGnu board in ../gcc/dejagnu looks for it beside the binary
# (see uknc-run), so the machine's own firmware goes in next to it.
cp ${BUILDDIR}/../rom/uknc_rom.bin ${BUILDDIR}/bin/

echo
echo "Built ${BUILDDIR}/bin/ukncbtldebug"
