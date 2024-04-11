# Kernel Command Line Options

A `cmdline` is a argv-like string that's passed to the kernel by the bootloader.
It can be used to configure the kernel at runtime. The kernel will parse the given
cmdline and apply the options specified.

Command line options that are not recognized by the kernel will be appended to the arguments
of init program.

## General Options

| Option              | Argument            | Description                                                                                                                 |
| ------------------- | ------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `init=`             | `<path>`            | Path to the init program, overrides the default init program.                                                               |
| `init_args=`        | `<args>`            | Arguments to pass to the init program, if multiple arguments are to be passed, separated by a space and enclosed in quotes. |
| `printk_console=`   | `<name>`/`<prefix>` | The name of the console to use for kernel messages, a prefix-based search is used if the name is not an exact match.        |
| `profile_console=`  | `<name>`/`<prefix>` | When profiling is enabled, print the profiling information to this console.                                                 |
| `poweroff_on_panic` |                     | Power off the machine when the kernel panics, instead of halting.                                                           |
| `quiet`             |                     | Disable most of the kernel messages, except for warnings and panics.                                                        |

## Unit Tests Options

These options are only useful if you are running MOS tests, (i.e. `BUILD_TESTING` is set to `ON` when building the kernel).

| Option                      | Argument   | Description                                                                 |
| --------------------------- | ---------- | --------------------------------------------------------------------------- |
| `mos_tests`                 |            | Run MOS tests.                                                              |
| `mos_tests_skip_prefix`     | `<prefix>` | Skip tests with the given prefix.                                           |
| `mos_tests_halt_on_success` |            | Halt the kernel after all tests have passed, instead of continuing to boot. |
