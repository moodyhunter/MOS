# 3. kallsyms - The Kernel Symbol Table

The `kallsyms` provides a way to access the kernel symbol table, i.e. the list of functions exported
by the kernel. This is useful for debugging, e.g. when the kernel crashes, a backtrace can be
generated using the symbol table.

## How is the symbol table merged into the kernel binary?

The CMake file `prepare_bootable_kernel_binary.cmake` contains a function `create_bootable_kernel_binary`.

In that function, the kernel is compiled for 3 times, using the following steps:

1. **COMPILE 1**: Firstly, compile the kernel with a stub empty `mos_kallsyms`.

   This stub contains nothing useful but to successfully link the kernel binary (avoiding undefined symbols).

2. **GEN 1**: Then, generate a map file from the kernel binary.

   This map file contains the addresses of all the symbols in the kernel binary.

   We then use the script `scripts/gen_kallsyms.py` to convert the map to a C file (e.g. `kallsyms.1.c`)
   which contains the symbol table in a kernel-readable format.

3. **COMPILE 2**: Re-compile the kernel with the generated `kallsyms.1.c` file.

4. **GEN 2**: Re-generate the map file from the kernel binary.

   Because the kernel binary is now larger, it's likely that the addresses of the symbols have changed.

   We then convert the map to another C file (e.g. `kallsyms.2.c`) using the script.

5. **COMPILE 3**: Re-compile the kernel with the generated `kallsyms.2.c` file.

   This time, the kernel binary should be the same size and be at the same address as the original one
   because no new symbols are added to the kernel binary.

6. The kernel binary is now ready to be booted.
