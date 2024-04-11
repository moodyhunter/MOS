# Kernel Command Line

## Early Options vs Normal Options

Options that are tagged with `MOS_EARLY_SETUP` are parsed before the kernel is loaded,
more specifically, is parsed as soon as the kernel takes control of the system.

> [!Note]
> Since the kernel isn't fully functional when early options are
> interpreted, most of subsystems are not yet initialized. This means that
> early options should be kept to a minimum and should only be used to configure
> the most basic and essential parts of the kernel.

## List of Options

The list of options can be extracted from the source code using the following command:

```sh
grep --exclude-dir build/ -Er '^MOS(_EARLY)?_SETUP' .
```
