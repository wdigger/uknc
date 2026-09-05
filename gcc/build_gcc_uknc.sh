#!/usr/bin/env sh
set -e

BINUTILS_VERSION="2.45"
GCC_VERSION="15.2.0"
NEWLIB_VERSION="4.6.0.20260123"

BUILDDIR="${PWD}"

# Preparing folders
cd ${BUILDDIR}
mkdir src
mkdir bin
mkdir xgcc

# Download, patch and build binutils
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz | tar -C ${BUILDDIR}/src -zxf -

curl https://github.com/wdigger/binutils-gdb/commit/09c5f5bc048d3e7d96b93efba699ceae742da2b0.patch -o binutils_1.patch
curl https://github.com/wdigger/binutils-gdb/commit/540689194f9fa20c6afe18aeee17630eb1f3b76c.patch -o binutils_2.patch
curl https://github.com/wdigger/binutils-gdb/commit/26ad15d4a79361f4d322c5a89e61b339a154cef1.patch -o binutils_3.patch
curl https://github.com/wdigger/binutils-gdb/commit/2cfcf47acd27eea618556f00336990ceb8ab82ed.patch -o binutils_4.patch
curl https://github.com/wdigger/binutils-gdb/commit/12a9ac2140efb341d65c863ec5d4d3ae0259b17c.patch -o binutils_5.patch

cd ${BUILDDIR}/src/binutils-${BINUTILS_VERSION}
patch -p1 < ${BUILDDIR}/binutils_1.patch
patch -p1 < ${BUILDDIR}/binutils_2.patch
patch -p1 < ${BUILDDIR}/binutils_3.patch
patch -p1 < ${BUILDDIR}/binutils_4.patch
patch -p1 < ${BUILDDIR}/binutils_5.patch
rm ${BUILDDIR}/binutils_1.patch
rm ${BUILDDIR}/binutils_2.patch
rm ${BUILDDIR}/binutils_3.patch
rm ${BUILDDIR}/binutils_4.patch
rm ${BUILDDIR}/binutils_5.patch

cd ${BUILDDIR}
mkdir -p build/binutils
cd build/binutils
${BUILDDIR}/src/binutils-${BINUTILS_VERSION}/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --disable-libstdcxx --disable-doc --with-system-zlib
make -j4 MAKEINFO=true && make install MAKEINFO=true

# Download and patch gcc
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz | tar -C ${BUILDDIR}/src -zxf -

curl https://github.com/wdigger/gcc/commit/4e983b0232e8866a77efeb294d49f8ea166dc0e7.patch -o gcc_1.patch
curl https://github.com/wdigger/gcc/commit/b6a22d2cc567af01c9847c20a1be508709f003f2.patch -o gcc_2.patch
curl https://github.com/wdigger/gcc/commit/6a68da64eff59984f2a294beb0b64af540684ebf.patch -o gcc_3.patch
curl https://github.com/wdigger/gcc/commit/83de41f666e1b826c460ade01f62e1a08d47abaf.patch -o gcc_4.patch
curl https://github.com/wdigger/gcc/commit/f2e04ca7b2367bba0496c214c68514e3fa1a7704.patch -o gcc_5.patch
curl https://github.com/wdigger/gcc/commit/24515e85ed970c217555cf8f2d2c11fa1ffbfd25.patch -o gcc_6.patch
curl https://github.com/wdigger/gcc/commit/c20c9b622ca597e17c2684071c7f38dfb5059037.patch -o gcc_7.patch
curl https://github.com/wdigger/gcc/commit/7d0a64e14effd6a8307e4da5fc8aedada1723269.patch -o gcc_8.patch
curl https://github.com/wdigger/gcc/commit/f2f0dea373eb9e38a668f3e85cfe67a331d2ef42.patch -o gcc_9.patch

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}
patch -p1 < ${BUILDDIR}/gcc_1.patch
patch -p1 < ${BUILDDIR}/gcc_2.patch
patch -p1 < ${BUILDDIR}/gcc_3.patch
patch -p1 < ${BUILDDIR}/gcc_4.patch
patch -p1 < ${BUILDDIR}/gcc_5.patch
patch -p1 < ${BUILDDIR}/gcc_6.patch
patch -p1 < ${BUILDDIR}/gcc_7.patch
patch -p1 < ${BUILDDIR}/gcc_8.patch
patch -p1 < ${BUILDDIR}/gcc_9.patch
rm ${BUILDDIR}/gcc_1.patch
rm ${BUILDDIR}/gcc_2.patch
rm ${BUILDDIR}/gcc_3.patch
rm ${BUILDDIR}/gcc_4.patch
rm ${BUILDDIR}/gcc_5.patch
rm ${BUILDDIR}/gcc_6.patch
rm ${BUILDDIR}/gcc_7.patch
rm ${BUILDDIR}/gcc_8.patch
rm ${BUILDDIR}/gcc_9.patch

# Download and patch newlib
cd ${BUILDDIR}

# newlib is unpacked directly into the gcc source tree (its own documented
# combined-tree convention: gcc's own top-level configure/Makefile.def
# already know how to build a "newlib" target module if the directory is
# present and --with-newlib is passed -- see gcc/config.gcc's
# pdp11-uknc-rt11 comment and newlib/libc/sys/rt11/ for the actual port).
curl https://sourceware.org/pub/newlib/newlib-${NEWLIB_VERSION}.tar.gz | tar -C ${BUILDDIR}/src -zxf -
cp -R ${BUILDDIR}/src/newlib-${NEWLIB_VERSION}/newlib ${BUILDDIR}/src/gcc-${GCC_VERSION}/newlib

curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/4bfd315ea9955ae2915040d5ff4cd3eabb9f9e6e.patch -o newlib_1.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/bc9de697a22a121561f422cd7c68e52cbdb84c62.patch -o newlib_2.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/8ec5d302b54206f2d4e0aaba20ac6c3d910dcc6b.patch -o newlib_3.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/930a14e6d52128ddfb92f1d7a87d76b6a4f77bfe.patch -o newlib_4.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/d555cd2dc7d36f9f3381e8491565d306efa585f6.patch -o newlib_5.patch
curl https://github.com/wdigger/sourceware-mirror-newlib-cygwin/commit/d01a026f73060db90afc9af2ea986549da41a715.patch -o newlib_6.patch

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}
patch -p1 < ${BUILDDIR}/newlib_1.patch
patch -p1 < ${BUILDDIR}/newlib_2.patch
patch -p1 < ${BUILDDIR}/newlib_3.patch
patch -p1 < ${BUILDDIR}/newlib_4.patch
patch -p1 < ${BUILDDIR}/newlib_5.patch
patch -p1 < ${BUILDDIR}/newlib_6.patch
rm ${BUILDDIR}/newlib_1.patch
rm ${BUILDDIR}/newlib_2.patch
rm ${BUILDDIR}/newlib_3.patch
rm ${BUILDDIR}/newlib_4.patch
rm ${BUILDDIR}/newlib_5.patch
rm ${BUILDDIR}/newlib_6.patch

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
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz | tar -C ${BUILDDIR}/src -zxf -

cd ${BUILDDIR}/src/autoconf-2.69
./configure --prefix "${BUILDDIR}/autotools"
make && make install

cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.gz | tar -C ${BUILDDIR}/src -zxf -
cd ${BUILDDIR}/src/automake-1.15.1
PATH="${BUILDDIR}/autotools/bin:${PATH}" ./configure --prefix "${BUILDDIR}/autotools"
PATH="${BUILDDIR}/autotools/bin:${PATH}" make && make install

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}/newlib
PATH="${BUILDDIR}/autotools/bin:${PATH}" autoreconf

# Build gcc
cd ${BUILDDIR}/src/gcc-${GCC_VERSION}

./contrib/download_prerequisites

cd ${BUILDDIR}
mkdir -p build/gcc
cd build/gcc
${BUILDDIR}/src/gcc-${GCC_VERSION}/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --enable-languages=c --with-gnu-as --with-gnu-ld --with-newlib --enable-newlib-nano-malloc --enable-newlib-nano-formatted-io --disable-newlib-wide-orient --disable-libssp --disable-bootstrap --disable-multilib --disable-nls --disable-libstdcxx --disable-doc --with-system-zlib --disable-libquadmath
make -j4 MAKEINFO=true && make install MAKEINFO=true

# Download and build rt11dsk
cd ${BUILDDIR}/src
git clone https://github.com/nzeemin/ukncbtl-utils.git
cd ${BUILDDIR}/src/ukncbtl-utils/rt11dsk
make
cp rt11dsk ${BUILDDIR}/bin/rt11dsk

# Build and install libppu (../libs/libppu): installs libppu.a, its
# headers (ppu_client.h/ppu_server.h), and ppu.ld into the sysroot
# alongside libc.a, so any CPU-side program can just link with -lppu,
# the same way the toolchain's own libc/libm/libg already work; also
# installs pdp11-uknc-rt11-ld-ppu, a small wrapper around the real
# pdp11-uknc-rt11-ld, into ${BUILDDIR}/bin alongside every other
# pdp11-uknc-rt11-* tool -- the linker a PPU-side program should use
# instead, with libppu's own linking requirements (ppu.ld, -u start,
# -lppu) already built in.
cd ${BUILDDIR}/../libs/libppu
PATH="${BUILDDIR}/bin:${PATH}" make install

# Build and install libpdp11 (../libs/libpdp11): plain-PDP-11
# primitives (interrupt priority mask/unmask, interrupt vector
# get/set/swap -- see its own pdp11_irq.h) shared by both CPU- and
# PPU-side programs -- nothing here is UKNC-specific (unlike libppu,
# which is this project's own CPU<->PPU protocol), so it needs no
# special linker wrapper; installs libpdp11.a and pdp11_irq.h into the
# sysroot the same way libc.a/libppu.a already are, so any program just
# links with -lpdp11.
cd ${BUILDDIR}/../libs/libpdp11
PATH="${BUILDDIR}/bin:${PATH}" make install
