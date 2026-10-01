#!/usr/bin/env sh
set -e

# Builds the pdp11-uknc-rt11 cross toolchain: binutils, gcc and newlib
# from their vanilla releases, with this project's own patches applied
# from patches/ beside this script -- one directory per component, one
# file per commit.  They live there and nowhere else: a build needs
# GitHub for the release tarballs alone, what is applied is what the
# repository says it is, and a change to a patch is a change with a diff
# like any other.
#
# The first thirty-odd came from the forks
#
#   binutils  wdigger/binutils-gdb  topic/rt11-sav-pdp11
#   gcc       wdigger/gcc           topic/1801bm1-gcc15.2
#   newlib    wdigger/sourceware-…  rt11-port
#
# which are no longer kept up to date -- except that gdb, having no
# release of its own to patch, is still built from a clone of that
# binutils branch, and takes its bfd from the clone.  So a patch under
# patches/binutils that gdb needs too has to reach the branch as well.
#
# Objects are ELF.  They were a.out until 2026-09-14, which is why the
# patch lists below have a second half: a.out has three sections and no
# way to name a fourth, so -ffunction-sections was refused outright,
# --gc-sections had nothing to collect, -g produced nothing and -flto
# was impossible.  The last a.out state of each branch is frozen at
# topic/rt11-sav-pdp11-aout and topic/1801bm1-gcc15.2-aout if it is ever
# wanted back; the two produce byte-identical programs as long as
# --gc-sections is off, which is how the change was checked.

BINUTILS_VERSION="2.45"
GCC_VERSION="15.2.0"
NEWLIB_VERSION="4.6.0.20260123"

BUILDDIR="${PWD}"
# Where this script is, which is where its patches are.  Not the same as
# BUILDDIR: the build can be run from anywhere, the patches cannot move.
SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"

# Where the sources are, as the configure scripts are told.
#
# On MSYS2 that is relative to the build directory, and this is the
# whole trick to building here.  What comes out is native Windows
# programs, and the build runs some of them on paths it wrote down for
# itself: gengtype reads its input list out of a file, where the MSYS
# runtime is not there to translate /d/a/... into something Windows
# knows, and it says there is no such file.  Writing D:/a/... instead
# fixes that and breaks make, which reads the colon in a rule as its own
# separator ("target pattern contains no '%'", in gettext's Makefiles).
# A relative path is neither: no drive letter for make to trip on, and
# nothing to translate for gengtype.  Elsewhere, absolute as before.
SRCPREFIX="${BUILDDIR}/src"
if command -v cygpath > /dev/null 2>&1; then
	SRCPREFIX="../../src"
fi

# `curl ... | tar` hides a failed download: set -e sees only tar's exit
# status, so a truncated stream leaves a half-extracted tree behind and
# the script carries on to build against it.  Fetch to a file first.
fetch_and_extract () {
	curl -fL "$1" -o "${BUILDDIR}/tarball.tmp"
	tar -C "$2" -zxf "${BUILDDIR}/tarball.tmp"
	rm -f "${BUILDDIR}/tarball.tmp"
}

# The patches are this project's own, and they live beside this script:
# patches/<component>/, one file per commit, applied in the order their
# names sort in.  Each is a fork commit exported with "format-patch", so
# its first line still names the commit it came from -- that is how one
# is traced back to the branch it belongs to.
# -E, because some of these delete a file: without it patch leaves the
# file behind with nothing in it, which GNU patch would have removed and
# BSD patch does not -- and an empty .c in the testsuite is a test.
apply_patches () {
	for p in "${SCRIPTDIR}"/patches/"$1"/*.patch; do
		echo "--- $(basename "${p}")"
		patch -E -p1 < "${p}"
	done
}

# Preparing folders
cd ${BUILDDIR}
mkdir src
mkdir bin
mkdir xgcc

# Download, patch and build binutils
cd ${BUILDDIR}
fetch_and_extract https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz ${BUILDDIR}/src

cd ${BUILDDIR}/src/binutils-${BINUTILS_VERSION}
apply_patches binutils

cd ${BUILDDIR}
mkdir -p build/binutils
cd build/binutils
# --enable-plugins is what lets ld load gcc's liblto_plugin.so, and so
# what makes -flto work: without it ld reports "-plugin PLUGIN
# (ignored)", sees an LTO object as an empty file with a
# __gnu_lto_slim marker in it, and the link fails on an undefined main.
# A cross binutils does not enable it on its own.
# Two statements rather than "make && make install": under set -e a
# command on the left of && is exempt, so a failed build went quietly on
# to install nothing and the script carried on to the next package --
# which is how a broken gcc reached the point of compiling libppu with a
# compiler that was never installed.
${SRCPREFIX}/binutils-${BINUTILS_VERSION}/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --enable-plugins --disable-libstdcxx --disable-doc --with-system-zlib
make -j4 MAKEINFO=true
make install MAKEINFO=true

# Clone and build gdb
#
# gdb is not in the binutils release tarball -- it shares a repository
# with binutils but not a release -- and its pdp11 support is this
# project's own (gdb/pdp11-tdep.c, on the same fork branch as the
# binutils patches above).  So this one comes from the branch directly
# rather than as a patch over a tarball: the branch already has the bfd
# side of the ELF work applied, and applying it twice to a second
# tarball would be the only alternative.  The clone is about 600MB.
#
# Everything but gdb is switched off here; binutils, gas and ld are the
# ones built above, from the release.
cd ${BUILDDIR}
git clone --depth 1 --single-branch --branch topic/rt11-sav-pdp11 https://github.com/wdigger/binutils-gdb.git ${BUILDDIR}/src/gdb

# gdb, alone among these, wants GMP and MPFR at build time -- it does
# target arithmetic with them.  Its configure looks where the compiler
# looks, so a package manager that installs outside that has to be
# pointed at; on a distribution that puts them in /usr this loop finds
# nothing and there is nothing to point at.
GDB_MATH=""
for prefix in /opt/local /opt/homebrew /usr/local; do
	if [ -f "${prefix}/include/gmp.h" ] && [ -f "${prefix}/include/mpfr.h" ]; then
		GDB_MATH="--with-gmp=${prefix} --with-mpfr=${prefix}"
		break
	fi
done

# gdb's Python is not a nicety here: libs/libppu/ppu.gdb is written in
# it, and that script is how the PPU side gets its symbols.  configure
# switches Python off rather than stopping when the one it found cannot
# be linked against, and the only sign is a line in config.log -- so a
# rebuild loses PPU debugging without saying anything.  macOS is exactly
# that case: /usr/bin/python3 is Apple's, and its python3-config reports
# the framework as a relative path, which nothing can link.  Hence a
# package manager's Python first, where there is one.  Where there is
# not -- a distribution with python3-dev installed -- configure finds it
# by itself and this adds nothing.
GDB_PYTHON=""
for python in \
	/opt/homebrew/opt/python@3*/bin/python3 \
	/opt/local/bin/python3 \
	/usr/local/opt/python@3*/bin/python3
do
	if [ -x "${python}" ]; then
		GDB_PYTHON="--with-python=${python}"
		echo "gdb: building against ${python}"
		break
	fi
done

mkdir -p ${BUILDDIR}/build/gdb
cd ${BUILDDIR}/build/gdb
# CC/CXX are pinned to the platform's own cc/c++ rather than left to
# configure, which prefers gcc/g++ wherever it finds them.  On macOS
# with MacPorts gcc installed that pairs a GCC front end with Apple's
# linker, and gdb is the one thing here that trips it: the link of the
# Fortran expression parser fails with "invalid r_symbolnum ... in
# f-exp.o".  On Linux cc and c++ are gcc and g++ anyway.
CC="${CC:-cc}" CXX="${CXX:-c++}" ${SRCPREFIX}/gdb/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --disable-binutils --disable-gas --disable-ld --disable-gold --disable-gprof --disable-gprofng --disable-sim --disable-nls --disable-werror --disable-doc --with-system-zlib ${GDB_MATH} ${GDB_PYTHON}
make all-gdb -j4 MAKEINFO=true
make install-gdb MAKEINFO=true

# Said out loud, because the alternative is finding out when a
# breakpoint in PPU code cannot be set.
if "${BUILDDIR}/bin/pdp11-uknc-rt11-gdb" -batch -ex 'python pass' \
		> /dev/null 2>&1; then
	echo "gdb: Python is there, so ppu.gdb will load"
else
	echo "gdb: WARNING -- no Python; libs/libppu/ppu.gdb will not load," \
		"and the PPU side has no symbols"
fi

# Download and patch gcc
cd ${BUILDDIR}
fetch_and_extract https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz ${BUILDDIR}/src

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}
apply_patches gcc

# Download and patch newlib
cd ${BUILDDIR}

# newlib is unpacked directly into the gcc source tree (its own documented
# combined-tree convention: gcc's own top-level configure/Makefile.def
# already know how to build a "newlib" target module if the directory is
# present and --with-newlib is passed -- see gcc/config.gcc's
# pdp11-uknc-rt11 comment and newlib/libc/sys/rt11/ for the actual port).
fetch_and_extract https://sourceware.org/pub/newlib/newlib-${NEWLIB_VERSION}.tar.gz ${BUILDDIR}/src
cp -R ${BUILDDIR}/src/newlib-${NEWLIB_VERSION}/newlib ${BUILDDIR}/src/gcc-${GCC_VERSION}/newlib

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}
apply_patches newlib

# newlib patch 0001 touches configure.host/libc/acinclude.m4, 0004
# touches libc/sys/rt11/Makefile.inc, and 0005 touches
# configure.host again -- so configure and Makefile.in must be
# regenerated from them -- but newlib's own shipped Makefile.in says it
# was generated by automake 1.15.1, and a newer automake changes
# per-object filename prefixes in a way that breaks newlib's own
# hardcoded MATHOBJS_IN_LIBC list (libc.a silently fails at "ar": "no entry
# ... in archive"). Pin both autoconf and automake to the exact versions
# newlib itself was built with, rather than whatever the system happens to
# have, and regenerate with those.
cd ${BUILDDIR}
AUTOTOOLS="${BUILDDIR}/autotools"

fetch_and_extract https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz ${BUILDDIR}/src

cd ${BUILDDIR}/src/autoconf-2.69
./configure --prefix "${AUTOTOOLS}"
make
make install

cd ${BUILDDIR}
fetch_and_extract https://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.gz ${BUILDDIR}/src
cd ${BUILDDIR}/src/automake-1.15.1
PATH="${AUTOTOOLS}/bin:${PATH}" ./configure --prefix "${AUTOTOOLS}"
PATH="${AUTOTOOLS}/bin:${PATH}" make
PATH="${AUTOTOOLS}/bin:${PATH}" make install

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}/newlib
PATH="${AUTOTOOLS}/bin:${PATH}" autoreconf

# Build gcc
cd ${BUILDDIR}/src/gcc-${GCC_VERSION}

# GCC wants GMP, MPFR and MPC, and ships a script that fetches them into
# its own tree.  Where the host already has them, use those: on MSYS2
# the in-tree GMP fails its own "Oops, mp_limb_t doesn't seem to work"
# check and takes the whole build with it, while the packaged one builds
# gcc perfectly well.
if printf '#include <gmp.h>\n#include <mpfr.h>\n#include <mpc.h>\n' \
	| ${CC:-cc} -E - > /dev/null 2>&1; then
	echo "GMP, MPFR and MPC: using the host's own"
else
	./contrib/download_prerequisites
fi

# GCC 15's own libcody writes u8"..." where it means const char*, which
# stopped being true in C++20 -- so a host compiler that defaults to
# C++20 or later (MSYS2 ships GCC 16) cannot build it.
#
# The fix is that one feature and not the standard: libcody's own
# configure insists on __cplusplus being exactly 201103 and adds
# -std=c++11 itself when it is not, so pinning a standard here only
# talks over it (-std=gnu++17 in CXXFLAGS comes after libcody's own
# -std=c++11 on the command line and wins, and its configure then fails
# outright with "C++11 is required").  -fno-char8_t changes what u8"x"
# is and nothing else.
GCC_CXX_FLAGS=""
if ! printf 'const char *p = u8"x";\n' \
	| ${CXX:-c++} -x c++ -fsyntax-only - > /dev/null 2>&1; then
	GCC_CXX_FLAGS="-fno-char8_t"
	echo "host C++ is newer than gcc ${GCC_VERSION} expects: adding ${GCC_CXX_FLAGS}"
fi

cd ${BUILDDIR}
mkdir -p build/gcc
cd build/gcc
CXXFLAGS="${CXXFLAGS:--g -O2} ${GCC_CXX_FLAGS}" ${SRCPREFIX}/gcc-${GCC_VERSION}/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --enable-languages=c --with-gnu-as --with-gnu-ld --with-newlib --enable-newlib-nano-malloc --enable-newlib-nano-formatted-io --disable-newlib-wide-orient --disable-libssp --disable-bootstrap --disable-multilib --disable-nls --disable-libstdcxx --disable-doc --with-system-zlib --disable-libquadmath
# CFLAGS_FOR_TARGET carries -ffunction-sections/-fdata-sections into
# newlib and libgcc.  A program only links the library members it needs,
# but a member is a whole file, and one function of it is usually all
# that gets called; with a section per function the linker drops the
# rest.  It is worth a good deal here -- tests/fileio goes from 9200 to
# 6728 bytes on it alone -- and costs nothing at run time.
make -j4 MAKEINFO=true CFLAGS_FOR_TARGET="-g -O2 -ffunction-sections -fdata-sections"
make install MAKEINFO=true CFLAGS_FOR_TARGET="-g -O2 -ffunction-sections -fdata-sections"

# Download and build rt11dsk
cd ${BUILDDIR}/src
git clone https://github.com/nzeemin/ukncbtl-utils.git
cd ${BUILDDIR}/src/ukncbtl-utils/rt11dsk
make
cp rt11dsk ${BUILDDIR}/bin/rt11dsk

# Build and install libppu (../libs/libppu): libppu.a, its headers
# (ppu_client.h/ppu_server.h) and ppu.ld go into the sysroot beside
# libc.a, so a CPU-side program links with plain -lppu; and
# pdp11-uknc-rt11-ld-ppu, a wrapper around the real linker carrying
# libppu's own linking requirements (ppu.ld, -u start, -lppu), goes into
# bin beside every other pdp11-uknc-rt11-* tool.
cd ${BUILDDIR}/../libs/libppu
PATH="${BUILDDIR}/bin:${PATH}" make install

# Build and install libpdp11 (../libs/libpdp11): plain-PDP-11 primitives
# (interrupt priority mask/unmask, interrupt vector get/set/swap -- see
# its pdp11_irq.h) shared by CPU- and PPU-side programs.  Nothing here is
# UKNC-specific, so it needs no wrapper of its own: -lpdp11 is enough.
cd ${BUILDDIR}/../libs/libpdp11
PATH="${BUILDDIR}/bin:${PATH}" make install
