# Some Random Stuff

## "Cross-Compiler"

For someone who argues that some magic flags with a native compiler is enough to build a bare-metal OS:

<details>
<summary>Click to expand</summary>

Nice try, but I've tried it and it gives me more problems than it solves.

Because we are targeting the 32-bit x86 architecture, for most of the time, with a
x86_64 host machine and a native compiler, `-m32` will be required to produce a
32-bit binary.

This causes problems when linking, then you add `-melf_i386` to the linker.

And then comes the problem of a non-x86 host machine, for example, aarch64 (Say Hi to Apple M1/2 Chip!). There's no way you can produce a 32-bit x86 binary using that compiler by simply adding a `-m32`.

Another problem is the `-nostdinc` flag, which is required to prevent the compiler from searching for headers in you native include directories (say `/usr/include` on a "regular?" Linux machine). This break everything, you'll have to roll your own `stdbool.h`, `stdarg.h` and `stddef.h` which defines `NULL`, `size_t`, `va_list` and `bool`.

There will be even more problems like `libgcc`, `stack-protector`, using `ld` instead of a compiler launcher as linker...

After all, **please** install a cross-compiler, once and for all :)

</details>
