#!/usr/bin/env python

import io
import os
from sys import argv, stderr

"""
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/kallsyms.h"

const kallsyms_t mos_kallsyms[] = {
    { .name = NULL, .address = 0 },
};
"""

outfile: io.TextIOBase = None


def gen(str):
    outfile.write(str + "\n")


def main():
    global outfile

    if len(argv) != 3:
        print("Usage: %s <kernel.map> <output-kallsyms.c>" % argv[0])
        exit(1)

    mapfile = argv[1]
    output = argv[2]

    if output == "-":
        outfile = os.fdopen(os.dup(1), "w")
    else:
        outfile = open(output, "w")

    lines = []
    with open(mapfile, "r") as f:
        lines = f.readlines()

    gen("// SPDX-License-Identifier: GPL-3.0-or-later")
    gen("")
    gen('#include "mos/misc/kallsyms.h"')
    gen("")
    gen("const kallsyms_t mos_kallsyms[] = {")

    should_skip = True

    for l in lines:
        l = l.strip()

        count = l.count(" ")
        if count < 2:
            print("", file=stderr)
            print("Failed to generate ELF map info:", file=stderr)
            print("    Invalid line: '%s', expected 2 fields, got %d" % (l, count), file=stderr)
            print("", file=stderr)
            exit(1)

        (addr, type, name) = l.split(" ", 2)

        # Skip everything before __MOS_KERNEL_CODE_START, because we don't need
        # to export symbols from the bootloader.
        if name == "__MOS_KERNEL_CODE_START":
            should_skip = False

        if should_skip:
            continue

        if type == "t" or type == "T":
            gen("    { .address = 0x%s, .name = %s }," %
                (addr, '"' + name + '"'))

    gen("    { .address = 0, .name = NULL },")
    gen("};")
    pass


if __name__ == "__main__":
    main()
