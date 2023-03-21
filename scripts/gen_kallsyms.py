#!/usr/bin/env python

import io
import os
from sys import argv

"""
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kallsyms.h"

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
    gen("#include \"mos/kallsyms.h\"")
    gen("")
    gen("const kallsyms_t mos_kallsyms[] = {")

    for l in lines:
        l = l.strip()

        (addr, type, name) = l.split(" ", 2)

        if type == "t" or type == 'T':
            gen("    { .address = 0x%s, .name = %s }," % (addr, "\"" + name + "\""))

    gen("    { .address = 0, .name = NULL },")
    gen("};")
    pass


if __name__ == "__main__":
    main()
