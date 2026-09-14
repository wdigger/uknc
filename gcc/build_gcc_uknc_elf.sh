#!/usr/bin/env sh
set -e

# Builds the *second*, ELF-object toolchain, side by side with the one
# build_gcc_uknc.sh produces and without disturbing it: its own sources
# in src-elf, its own build tree in build-elf, its own sysroot in
# xgcc-elf, its own tools in bin-elf.  Both install their binaries under
# the same pdp11-uknc-rt11- prefix, so which one you get is decided by
# which of the two bin directories is on PATH.
#
# The triplet is deliberately the same.  gcc's config.gcc and newlib's
# configure.host both key on it (the "rt11" OS component is what selects
# libc/sys/rt11), and inventing a second triplet just to say "ELF" would
# mean touching both for no gain.  The object format is decided by the
# *branches* this script takes its patches from instead:
#
#   binutils  wdigger/binutils-gdb       topic/rt11-elf-pdp11
#   gcc       wdigger/gcc                topic/1801bm1-gcc15.2-elf
#   newlib    wdigger/sourceware-...     rt11-port   (unchanged so far)
#
# Each ELF branch starts at the tip of the a.out branch it came from, so
# at the beginning the two scripts apply the identical commits and the
# two toolchains are the same toolchain twice -- which is the point: it
# gives a known-good baseline to compare every later ELF change against
# (build the examples both ways, the .sav and .ppu files must match byte
# for byte until a change is meant to alter them).
#
# One exception to that, and it is not a defect: four libc.a members
# (dtoa, mprec, gdtoa-dmisc, gdtoa-gethex) carry an assert()'s __FILE__
# in .data, so their .data holds this script's src-elf path where the
# other holds src.  Their .text is identical, and nothing that links
# them ends up in an example.
#
# Note for whoever adds the next patch: a fix made on an a.out branch
# does *not* reach the ELF branch by itself.  Cherry-pick it there and
# add its own new SHA to this file too, or the two toolchains quietly
# stop being comparable.

BINUTILS_VERSION="2.45"
GCC_VERSION="15.2.0"
NEWLIB_VERSION="4.6.0.20260123"

BUILDDIR="${PWD}"

# Preparing folders
cd ${BUILDDIR}
mkdir src-elf
mkdir bin-elf
mkdir xgcc-elf

# Download, patch and build binutils
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz | tar -C ${BUILDDIR}/src-elf -zxf -

curl https://github.com/wdigger/binutils-gdb/commit/09c5f5bc048d3e7d96b93efba699ceae742da2b0.patch -o binutils_1.patch
curl https://github.com/wdigger/binutils-gdb/commit/540689194f9fa20c6afe18aeee17630eb1f3b76c.patch -o binutils_2.patch
curl https://github.com/wdigger/binutils-gdb/commit/26ad15d4a79361f4d322c5a89e61b339a154cef1.patch -o binutils_3.patch
curl https://github.com/wdigger/binutils-gdb/commit/2cfcf47acd27eea618556f00336990ceb8ab82ed.patch -o binutils_4.patch
curl https://github.com/wdigger/binutils-gdb/commit/12a9ac2140efb341d65c863ec5d4d3ae0259b17c.patch -o binutils_5.patch
curl https://github.com/wdigger/binutils-gdb/commit/c3826bc61b812b776b95ac64a320ed4dfeaba492.patch -o binutils_6.patch
curl https://github.com/wdigger/binutils-gdb/commit/e708665ce556d49513a5cb9c5ea69812bb4ef756.patch -o binutils_7.patch
# From here on, patches that exist only on topic/rt11-elf-pdp11.
curl https://github.com/wdigger/binutils-gdb/commit/71844960d2a0f3f1c7b9da975b311d88bb669515.patch -o binutils_8.patch
curl https://github.com/wdigger/binutils-gdb/commit/3c418668d2d87992c82af432728fbdf30ffa95f5.patch -o binutils_9.patch
curl https://github.com/wdigger/binutils-gdb/commit/55bffd5194aa0c95453b0a09042a38bf5fc4a907.patch -o binutils_10.patch
curl https://github.com/wdigger/binutils-gdb/commit/309a34cbb12e0dd70eccd957c7153b8e9c55cd1c.patch -o binutils_11.patch

cd ${BUILDDIR}/src-elf/binutils-${BINUTILS_VERSION}
patch -p1 < ${BUILDDIR}/binutils_1.patch
patch -p1 < ${BUILDDIR}/binutils_2.patch
patch -p1 < ${BUILDDIR}/binutils_3.patch
patch -p1 < ${BUILDDIR}/binutils_4.patch
patch -p1 < ${BUILDDIR}/binutils_5.patch
patch -p1 < ${BUILDDIR}/binutils_6.patch
patch -p1 < ${BUILDDIR}/binutils_7.patch
patch -p1 < ${BUILDDIR}/binutils_8.patch
patch -p1 < ${BUILDDIR}/binutils_9.patch
patch -p1 < ${BUILDDIR}/binutils_10.patch
patch -p1 < ${BUILDDIR}/binutils_11.patch
rm ${BUILDDIR}/binutils_1.patch
rm ${BUILDDIR}/binutils_2.patch
rm ${BUILDDIR}/binutils_3.patch
rm ${BUILDDIR}/binutils_4.patch
rm ${BUILDDIR}/binutils_5.patch
rm ${BUILDDIR}/binutils_6.patch
rm ${BUILDDIR}/binutils_7.patch
rm ${BUILDDIR}/binutils_8.patch
rm ${BUILDDIR}/binutils_9.patch
rm ${BUILDDIR}/binutils_10.patch
rm ${BUILDDIR}/binutils_11.patch

cd ${BUILDDIR}
mkdir -p build-elf/binutils
cd build-elf/binutils
${BUILDDIR}/src-elf/binutils-${BINUTILS_VERSION}/configure --prefix "${BUILDDIR}/xgcc-elf" --bindir "${BUILDDIR}/bin-elf" --target pdp11-uknc-rt11 --disable-libstdcxx --disable-doc --with-system-zlib
make -j4 MAKEINFO=true && make install MAKEINFO=true

# Download and patch gcc
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz | tar -C ${BUILDDIR}/src-elf -zxf -

curl https://github.com/wdigger/gcc/commit/4e983b0232e8866a77efeb294d49f8ea166dc0e7.patch -o gcc_1.patch
curl https://github.com/wdigger/gcc/commit/b6a22d2cc567af01c9847c20a1be508709f003f2.patch -o gcc_2.patch
curl https://github.com/wdigger/gcc/commit/6a68da64eff59984f2a294beb0b64af540684ebf.patch -o gcc_3.patch
curl https://github.com/wdigger/gcc/commit/83de41f666e1b826c460ade01f62e1a08d47abaf.patch -o gcc_4.patch
curl https://github.com/wdigger/gcc/commit/f2e04ca7b2367bba0496c214c68514e3fa1a7704.patch -o gcc_5.patch
curl https://github.com/wdigger/gcc/commit/24515e85ed970c217555cf8f2d2c11fa1ffbfd25.patch -o gcc_6.patch
curl https://github.com/wdigger/gcc/commit/c20c9b622ca597e17c2684071c7f38dfb5059037.patch -o gcc_7.patch
curl https://github.com/wdigger/gcc/commit/7d0a64e14effd6a8307e4da5fc8aedada1723269.patch -o gcc_8.patch
curl https://github.com/wdigger/gcc/commit/f2f0dea373eb9e38a668f3e85cfe67a331d2ef42.patch -o gcc_9.patch
curl https://github.com/wdigger/gcc/commit/3b581bf2b13a665f237b8c2c86aab5196769381d.patch -o gcc_10.patch
curl https://github.com/wdigger/gcc/commit/03e44cf1026bcb2a980f93faa6c6b32030334a5a.patch -o gcc_11.patch
curl https://github.com/wdigger/gcc/commit/b31e58a565675e94bdba0564d6fee3aac0944e08.patch -o gcc_12.patch
curl https://github.com/wdigger/gcc/commit/d7e5b14f25dc8d08b8f04e3c6bd517c2618b7073.patch -o gcc_13.patch
curl https://github.com/wdigger/gcc/commit/41d731baf394296b2bc0a2b93c2db103292138e7.patch -o gcc_14.patch
curl https://github.com/wdigger/gcc/commit/2f718d7f0290a628f5a0d8694cb53cae5516d9a4.patch -o gcc_15.patch
curl https://github.com/wdigger/gcc/commit/2c4b24b9634e41fc19754c6c34dfd338ccbb22cc.patch -o gcc_16.patch
curl https://github.com/wdigger/gcc/commit/94e56e9b8984c979b77df483a6f1ac7feda81d9a.patch -o gcc_17.patch
curl https://github.com/wdigger/gcc/commit/c81bf4ee927dcdcfc6758ead63fe3f758af15fd1.patch -o gcc_18.patch
curl https://github.com/wdigger/gcc/commit/3ba9daebb25ee1304a03965b8ddf4bf39ce37579.patch -o gcc_19.patch
curl https://github.com/wdigger/gcc/commit/5c5960e93efb3533eaa1f539c75fd7cd378589d0.patch -o gcc_20.patch
curl https://github.com/wdigger/gcc/commit/6855b8200eba8577a7937f75a73c7002dc3ea7ef.patch -o gcc_21.patch
curl https://github.com/wdigger/gcc/commit/e5432c3a7835f7541c7ca78bc7a1429bd2cfeb82.patch -o gcc_22.patch
# From here on, patches that exist only on topic/1801bm1-gcc15.2-elf.
curl https://github.com/wdigger/gcc/commit/78485b1287f758f315a2d7e4751f206b8fa3f586.patch -o gcc_23.patch

cd ${BUILDDIR}/src-elf/gcc-${GCC_VERSION}
patch -p1 < ${BUILDDIR}/gcc_1.patch
patch -p1 < ${BUILDDIR}/gcc_2.patch
patch -p1 < ${BUILDDIR}/gcc_3.patch
patch -p1 < ${BUILDDIR}/gcc_4.patch
patch -p1 < ${BUILDDIR}/gcc_5.patch
patch -p1 < ${BUILDDIR}/gcc_6.patch
patch -p1 < ${BUILDDIR}/gcc_7.patch
patch -p1 < ${BUILDDIR}/gcc_8.patch
patch -p1 < ${BUILDDIR}/gcc_9.patch
patch -p1 < ${BUILDDIR}/gcc_10.patch
patch -p1 < ${BUILDDIR}/gcc_11.patch
patch -p1 < ${BUILDDIR}/gcc_12.patch
patch -p1 < ${BUILDDIR}/gcc_13.patch
patch -p1 < ${BUILDDIR}/gcc_14.patch
patch -p1 < ${BUILDDIR}/gcc_15.patch
patch -p1 < ${BUILDDIR}/gcc_16.patch
patch -p1 < ${BUILDDIR}/gcc_17.patch
patch -p1 < ${BUILDDIR}/gcc_18.patch
patch -p1 < ${BUILDDIR}/gcc_19.patch
patch -p1 < ${BUILDDIR}/gcc_20.patch
patch -p1 < ${BUILDDIR}/gcc_21.patch
patch -p1 < ${BUILDDIR}/gcc_22.patch
patch -p1 < ${BUILDDIR}/gcc_23.patch
rm ${BUILDDIR}/gcc_1.patch
rm ${BUILDDIR}/gcc_2.patch
rm ${BUILDDIR}/gcc_3.patch
rm ${BUILDDIR}/gcc_4.patch
rm ${BUILDDIR}/gcc_5.patch
rm ${BUILDDIR}/gcc_6.patch
rm ${BUILDDIR}/gcc_7.patch
rm ${BUILDDIR}/gcc_8.patch
rm ${BUILDDIR}/gcc_9.patch
rm ${BUILDDIR}/gcc_10.patch
rm ${BUILDDIR}/gcc_11.patch
rm ${BUILDDIR}/gcc_12.patch
rm ${BUILDDIR}/gcc_13.patch
rm ${BUILDDIR}/gcc_14.patch
rm ${BUILDDIR}/gcc_15.patch
rm ${BUILDDIR}/gcc_16.patch
rm ${BUILDDIR}/gcc_17.patch
rm ${BUILDDIR}/gcc_18.patch
rm ${BUILDDIR}/gcc_19.patch
rm ${BUILDDIR}/gcc_20.patch
rm ${BUILDDIR}/gcc_21.patch
rm ${BUILDDIR}/gcc_22.patch
rm ${BUILDDIR}/gcc_23.patch

# Download and patch newlib
cd ${BUILDDIR}

# newlib is unpacked directly into the gcc source tree (its own documented
# combined-tree convention: gcc's own top-level configure/Makefile.def
# already know how to build a "newlib" target module if the directory is
# present and --with-newlib is passed -- see gcc/config.gcc's
# pdp11-uknc-rt11 comment and newlib/libc/sys/rt11/ for the actual port).
curl https://sourceware.org/pub/newlib/newlib-${NEWLIB_VERSION}.tar.gz | tar -C ${BUILDDIR}/src-elf -zxf -
cp -R ${BUILDDIR}/src-elf/newlib-${NEWLIB_VERSION}/newlib ${BUILDDIR}/src-elf/gcc-${GCC_VERSION}/newlib

curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/4bfd315ea9955ae2915040d5ff4cd3eabb9f9e6e.patch -o newlib_1.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/bc9de697a22a121561f422cd7c68e52cbdb84c62.patch -o newlib_2.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/8ec5d302b54206f2d4e0aaba20ac6c3d910dcc6b.patch -o newlib_3.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/930a14e6d52128ddfb92f1d7a87d76b6a4f77bfe.patch -o newlib_4.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/d555cd2dc7d36f9f3381e8491565d306efa585f6.patch -o newlib_5.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/d01a026f73060db90afc9af2ea986549da41a715.patch -o newlib_6.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/a3d569699dc78dfd3974af1560b80d71cc1c505e.patch -o newlib_7.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/21275ce2fb7390857455f8637db4fb70cfa1da68.patch -o newlib_8.patch

cd ${BUILDDIR}/src-elf/gcc-${GCC_VERSION}
patch -p1 < ${BUILDDIR}/newlib_1.patch
patch -p1 < ${BUILDDIR}/newlib_2.patch
patch -p1 < ${BUILDDIR}/newlib_3.patch
patch -p1 < ${BUILDDIR}/newlib_4.patch
patch -p1 < ${BUILDDIR}/newlib_5.patch
patch -p1 < ${BUILDDIR}/newlib_6.patch
patch -p1 < ${BUILDDIR}/newlib_7.patch
patch -p1 < ${BUILDDIR}/newlib_8.patch
rm ${BUILDDIR}/newlib_1.patch
rm ${BUILDDIR}/newlib_2.patch
rm ${BUILDDIR}/newlib_3.patch
rm ${BUILDDIR}/newlib_4.patch
rm ${BUILDDIR}/newlib_5.patch
rm ${BUILDDIR}/newlib_6.patch
rm ${BUILDDIR}/newlib_7.patch
rm ${BUILDDIR}/newlib_8.patch

# newlib_1.patch touches configure.host/libc/acinclude.m4, newlib_4.patch
# touches libc/sys/rt11/Makefile.inc, and newlib_5.patch touches
# configure.host again -- so configure and Makefile.in must be
# regenerated from them -- but newlib's own shipped Makefile.in says it
# was generated by automake 1.15.1, and a newer automake changes
# per-object filename prefixes in a way that breaks newlib's own
# hardcoded MATHOBJS_IN_LIBC list (libc.a silently fails at "ar": "no entry
# ... in archive"). Pin both autoconf and automake to the exact versions
# newlib itself was built with, rather than whatever the system happens to
# have, and regenerate with those.
#
# These two are ordinary host programs with nothing target-specific about
# them, so if the a.out script has already built them, reuse them rather
# than build a second identical copy.
cd ${BUILDDIR}
if [ -x "${BUILDDIR}/autotools/bin/autoconf" ] && [ -x "${BUILDDIR}/autotools/bin/automake" ]; then
	AUTOTOOLS="${BUILDDIR}/autotools"
else
	AUTOTOOLS="${BUILDDIR}/autotools-elf"

	curl https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz | tar -C ${BUILDDIR}/src-elf -zxf -

	cd ${BUILDDIR}/src-elf/autoconf-2.69
	./configure --prefix "${AUTOTOOLS}"
	make && make install

	cd ${BUILDDIR}
	curl https://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.gz | tar -C ${BUILDDIR}/src-elf -zxf -
	cd ${BUILDDIR}/src-elf/automake-1.15.1
	PATH="${AUTOTOOLS}/bin:${PATH}" ./configure --prefix "${AUTOTOOLS}"
	PATH="${AUTOTOOLS}/bin:${PATH}" make && make install
fi

cd ${BUILDDIR}/src-elf/gcc-${GCC_VERSION}/newlib
PATH="${AUTOTOOLS}/bin:${PATH}" autoreconf

# Build gcc
cd ${BUILDDIR}/src-elf/gcc-${GCC_VERSION}

./contrib/download_prerequisites

cd ${BUILDDIR}
mkdir -p build-elf/gcc
cd build-elf/gcc
${BUILDDIR}/src-elf/gcc-${GCC_VERSION}/configure --prefix "${BUILDDIR}/xgcc-elf" --bindir "${BUILDDIR}/bin-elf" --target pdp11-uknc-rt11 --enable-languages=c --with-gnu-as --with-gnu-ld --with-newlib --enable-newlib-nano-malloc --enable-newlib-nano-formatted-io --disable-newlib-wide-orient --disable-libssp --disable-bootstrap --disable-multilib --disable-nls --disable-libstdcxx --disable-doc --with-system-zlib --disable-libquadmath
# CFLAGS_FOR_TARGET carries -ffunction-sections/-fdata-sections into
# newlib and libgcc.  A program only links the library members it needs,
# but a member is a whole file, and one function of it is usually all
# that gets called; with a section per function the linker drops the
# rest.  It is worth a good deal here -- tests/fileio goes from 9200 to
# 6728 bytes on it alone -- and costs nothing at run time.
make -j4 MAKEINFO=true CFLAGS_FOR_TARGET="-g -O2 -ffunction-sections -fdata-sections" && make install MAKEINFO=true CFLAGS_FOR_TARGET="-g -O2 -ffunction-sections -fdata-sections"

# Download and build rt11dsk -- host-side and format-agnostic, so take
# the one the a.out build already produced if it is there.
cd ${BUILDDIR}
if [ -x "${BUILDDIR}/bin/rt11dsk" ]; then
	cp ${BUILDDIR}/bin/rt11dsk ${BUILDDIR}/bin-elf/rt11dsk
else
	cd ${BUILDDIR}/src-elf
	git clone https://github.com/nzeemin/ukncbtl-utils.git
	cd ${BUILDDIR}/src-elf/ukncbtl-utils/rt11dsk
	make
	cp rt11dsk ${BUILDDIR}/bin-elf/rt11dsk
fi

# Build and install libppu and libpdp11 into this toolchain's own
# sysroot.  Their Makefiles default SYSROOT/BINDIR to the a.out
# toolchain's directories, so both are overridden here; nothing else
# about them is format-specific.  See build_gcc_uknc.sh for what each
# library is and why the ld-ppu wrapper exists.
cd ${BUILDDIR}/../libs/libppu
PATH="${BUILDDIR}/bin-elf:${PATH}" make SYSROOT=../../gcc/xgcc-elf/pdp11-uknc-rt11 BINDIR=../../gcc/bin-elf install

cd ${BUILDDIR}/../libs/libpdp11
PATH="${BUILDDIR}/bin-elf:${PATH}" make SYSROOT=../../gcc/xgcc-elf/pdp11-uknc-rt11 install
