# Building MOS

This chapter assumes you already have a working cross-compiler toolchain, see
[Chapter 2: Prerequisites](02-prerequisites.md) if you don't.

We'll assume that your toolchain is installed in `$TOOLCHAIN/`.

## Before You Start

Please make sure `$TOOLCHAIN/bin/` as the first element of your `$PATH` (well, it
_can_ be anywhere, just make sure `i686-elf-gcc` is the one you've just built),
please also make sure `cmake`, `git`, `cpio` and `nasm` is in your `PATH`.

## CMake Configure

Just a simple command in the `build/` subdirectory:

```bash
mkdir build && cd build
cmake ..
```

If you see "MOS is now configured :)" then you're good to go.
