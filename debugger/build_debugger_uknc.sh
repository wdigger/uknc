#!/usr/bin/env sh
set -e

# Builds ukncbtl-debugger, the console debugger this project runs its
# programs in, from
#
#   https://github.com/wdigger/ukncbtl-debugger  branch gdbserver
#
# That branch is this project's own: the upstream console debugger with
# its own debugger taken out and gdb's remote protocol put in, so that
# pdp11-uknc-rt11-gdb drives the machine ("gdbserver" there, "target
# remote :2345" here).  Both processors are served, as gdb's two
# processes.
#
# Unlike the toolchain next door there is nothing to patch: the branch is
# the source, so this clones it rather than fetching commits one at a
# time over a release tarball.
#
# SDL3, if pkg-config finds it, gets --screen: the machine's screen in a
# window (brew install sdl3).  Nothing else needs it, and without it the
# build is the same build minus that option.
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
# emubase directly, and SDL3 is the one thing it can use and does not
# need -- so this is just its own Makefile, which puts the result in
# build/release.
cd ${BUILDDIR}/src
make release -j4

# Removed and not merely overwritten: macOS remembers that it has
# checked a binary's signature, and writing a new one over the same file
# leaves that memory wrong -- the next run is killed outright, with
# nothing said but "Killed: 9".
rm -f ${BUILDDIR}/bin/ukncbtldebug
cp ${BUILDDIR}/src/build/release/ukncbtldebug ${BUILDDIR}/bin/

# The emulator wants uknc_rom.bin in the current directory at run time,
# and the DejaGnu board in ../gcc/dejagnu looks for it beside the binary
# (see uknc-run), so the machine's own firmware goes in next to it.
cp ${BUILDDIR}/../rom/uknc_rom.bin ${BUILDDIR}/bin/

echo
echo "Built ${BUILDDIR}/bin/ukncbtldebug"
