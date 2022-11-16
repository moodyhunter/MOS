export PREFIX="/tmp"           # <--- CHANGE THIS
export MAKEFLAGS="-j$(nproc)"  # Change this to your liking
export BINUTILS_VERSION="2.39" # The latest version at the time of writing
export GCC_VERSION="12.2.0"    # The latest version at the time of writing
export GDB_VERSION="12.1"      # The latest version at the time of writing
export TARGET="i686-elf"       # The target triple

set -e # Exit on error

mkdir -p "$PREFIX"
cd "$PREFIX"
mkdir _work && cd _work

# Download and compile binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz
tar -xf binutils-$BINUTILS_VERSION.tar.gz

cd binutils-$BINUTILS_VERSION
mkdir build && cd build
../configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --disable-werror

make && make install
cd ../..

# Download and compile GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz
tar -xf gcc-$GCC_VERSION.tar.gz

cd gcc-$GCC_VERSION
mkdir build && cd build
../configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --disable-werror \
    --without-headers \
    --enable-libgcc \
    --enable-languages=c,c++ \
    --disable-build-format-warnings

# This will take a **long** time
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd ../..

# Download and compile GDB
wget https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.gz
tar -xf gdb-$GDB_VERSION.tar.gz

cd gdb-$GDB_VERSION
mkdir build && cd build
../configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --program-prefix="$TARGET-"

make && make install
cd ../..
cd .. # back to $PREFIX
