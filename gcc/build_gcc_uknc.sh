#!/usr/bin/env sh

BINUTILS_VERSION="2.45"
GCC_VERSION="15.2.0"

BUILDDIR="${PWD}"

# Preparing folders
cd ${BUILDDIR}
mkdir src
mkdir bin
mkdir xgcc

# Download, patch and build binutils
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz | tar -C ${BUILDDIR}/src -zxf -

curl https://github.com/wdigger/binutils-gdb/commit/b5e7fe5c4f0b90927f9ec3b891120e2ebedab958.patch -o binutils_1.patch

cd ${BUILDDIR}/src/binutils-${BINUTILS_VERSION}
patch -p1 < ${BUILDDIR}/binutils_1.patch
rm ${BUILDDIR}/binutils_1.patch

cd ${BUILDDIR}
mkdir -p build/binutils
cd build/binutils
${BUILDDIR}/src/binutils-${BINUTILS_VERSION}/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --disable-libstdcxx --disable-doc --with-system-zlib
make -j4 MAKEINFO=true && make install MAKEINFO=true

# Download, patch and build gcc
cd ${BUILDDIR}
curl https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz | tar -C ${BUILDDIR}/src -zxf -

curl https://github.com/wdigger/gcc/commit/4e983b0232e8866a77efeb294d49f8ea166dc0e7.patch -o gcc_1.patch
curl https://github.com/wdigger/gcc/commit/b6a22d2cc567af01c9847c20a1be508709f003f2.patch -o gcc_2.patch
curl https://github.com/wdigger/gcc/commit/6a68da64eff59984f2a294beb0b64af540684ebf.patch -o gcc_3.patch
curl https://github.com/wdigger/gcc/commit/83de41f666e1b826c460ade01f62e1a08d47abaf.patch -o gcc_4.patch
curl https://github.com/wdigger/gcc/commit/73fe78f386448f8ee5897418cbb8db51e5922444.patch -o gcc_5.patch

cd ${BUILDDIR}/src/gcc-${GCC_VERSION}
patch -p1 < ${BUILDDIR}/gcc_1.patch
patch -p1 < ${BUILDDIR}/gcc_2.patch
patch -p1 < ${BUILDDIR}/gcc_3.patch
patch -p1 < ${BUILDDIR}/gcc_4.patch
patch -p1 < ${BUILDDIR}/gcc_5.patch
rm ${BUILDDIR}/gcc_1.patch
rm ${BUILDDIR}/gcc_2.patch
rm ${BUILDDIR}/gcc_3.patch
rm ${BUILDDIR}/gcc_4.patch
rm ${BUILDDIR}/gcc_5.patch

./contrib/download_prerequisites

cd ${BUILDDIR}
mkdir -p build/gcc
cd build/gcc
${BUILDDIR}/src/gcc-${GCC_VERSION}/configure --prefix "${BUILDDIR}/xgcc" --bindir "${BUILDDIR}/bin" --target pdp11-uknc-rt11 --enable-languages=c --with-gnu-as --with-gnu-ld --disable-libssp --disable-bootstrap --disable-multilib --disable-nls --disable-libstdcxx --disable-doc --with-system-zlib --disable-libquadmath
make -j4 MAKEINFO=true && make install MAKEINFO=true

# Download and build rt11dsk
cd ${BUILDDIR}/src
git clone https://github.com/nzeemin/ukncbtl-utils.git
cd ${BUILDDIR}/src/ukncbtl-utils/rt11dsk
make
cp rt11dsk ${BUILDDIR}/bin/rt11dsk
