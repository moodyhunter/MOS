#!/bin/bash
#
# @AUTHOR :: Stjepan Bilić Matišić
# @ABOUT  :: Simple shell script to
#            build a cross-compiler
#

set -e

# Exports
export THIS_DIR=$(pwd)
export TARGET=i686-elf
export PREFIX="$THIS_DIR/toolchains/$TARGET"
export PATH="$PREFIX/bin:$PATH"

export BINUTILS_VERSION=2.39
export GCC_VERSION=12.2.0

export PARALLELISM=$(nproc)
export MAKEFLAGS="-j$PARALLELISM"

mkdir -p $PREFIX/_work
cd $PREFIX/_work

# Downloads

test -f binutils-$BINUTILS_VERSION.tar.xz || wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz
test -f gcc-$GCC_VERSION.tar.xz || wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz

# remove old work dirs
test -d binutils-$BINUTILS_VERSION && rm -rf binutils-$BINUTILS_VERSION
test -d binutils-build && rm -rf binutils-build
test -d gcc-$GCC_VERSION && rm -rf gcc-$GCC_VERSION
test -d gcc-build && rm -rf gcc-build

# Directory Setup
tar -xf binutils-$BINUTILS_VERSION.tar.xz
tar -xf gcc-$GCC_VERSION.tar.xz

# Binutils
mkdir binutils-build && cd binutils-build
../binutils-$BINUTILS_VERSION/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

make
make install
cd ..

# GCC
mkdir gcc-build && cd gcc-build
../gcc-$GCC_VERSION/configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --disable-werror \
    --without-headers \
    --enable-libgcc \
    --enable-languages=c,c++ \
    --disable-build-format-warnings

make all-gcc
make all-target-libgcc

make install-gcc
make install-target-libgcc
cd ..

# # "Perma" Export
# echo "# CROSS-COMPILER STUFF" >>~/.bashrc
# echo "export PREFIX="$HOME/Toolchain/i686-elf" " >>~/.bashrc
# echo "export TARGET=i686-elf" >>/.bashrc
# echo "export PATH="$PREFIX/bin:$PATH" " >>~/.bashrc
# echo "export TOOLCHAIN="$HOME/Toolchain/i686-elf" " >>~/.bashrc
