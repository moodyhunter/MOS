# librpc: argument encoding specification

This document describes the third argument to `rpc_simple_call()`, which is the
argument encoding specification. This is a string that describes the
arguments to the RPC call.

The string is a sequence of characters, each of which describes one type
of argument. The characters are:

- `v`: void, no argument, this must be the only character in the string if it is present
- `c`: 8-bit integer, `u8` or `unsigned char`
- `i`: 32-bit integer, `u32`
- `l`: 64-bit integer, `u64`
- `f`: double-precision floating point, `double`
- `s`: string, `const char *`

No other characters are allowed.
