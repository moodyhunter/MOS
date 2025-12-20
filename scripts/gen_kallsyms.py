#!/usr/bin/env python

import os
import subprocess
from sys import argv, stderr
from typing import IO


class KAllSymsEntry:
    address: str
    name: str
    demangled_name: str

    def __init__(self, address: str, name: str, demangled_name: str):
        self.address = address
        self.name = name
        self.demangled_name = demangled_name

    def get_pretty_name(self) -> str:
        reg = r'(::)?([\w]+)(<.*>)?\(.*\)?$'
        if self.demangled_name:
            import re
            match = re.search(reg, self.demangled_name)
            if match:
                return match.group(2)
        return ""

    def to_c_struct_line(self) -> str:
        pretty_name = self.get_pretty_name()
        return "    { .address = 0x%s, .name = %s, .demangled_name = %s, .pretty_name = %s }," % (
            self.address,
            '"' + self.name + '"',
            '"' + self.demangled_name + '"' if self.demangled_name else "NULL",
            '"' + pretty_name + '"' if pretty_name else "NULL"
        )


cppfilt_process = subprocess.Popen(
    ['c++filt'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)


def demangle(names: str) -> str:
    if cppfilt_process.stdin is None or cppfilt_process.stdout is None:
        print("c++filt process not properly initialized", file=stderr)
        exit(1)

    cppfilt_process.stdin.write(names + "\n")
    cppfilt_process.stdin.flush()
    demangled = cppfilt_process.stdout.readline().strip()
    return demangled


def main():
    global outfile

    if len(argv) != 3:
        print("Usage: %s <kernel.map> <output-kallsyms.c>" % argv[0])
        exit(1)

    entries = load_kernel_map(argv[1])
    outputFile = argv[2]
    outfile = os.fdopen(os.dup(1), "w") if outputFile == "-" else open(outputFile, "w")

    def gen(str):
        outfile.write(str + "\n")
        outfile.flush()

    gen("// SPDX-License-Identifier: GPL-3.0-or-later")
    gen("")
    gen('#include "mos/misc/kallsyms.hpp"')
    gen("")
    gen("const kallsyms_t mos_kallsyms[] = {")

    for e in entries:
        gen(e.to_c_struct_line())

    gen("    { .address = 0, .name = NULL, .demangled_name = NULL, .pretty_name = NULL },")
    gen("};")
    pass


def load_kernel_map(fileName: str) -> list[KAllSymsEntry]:
    entries: list[KAllSymsEntry] = []
    with open(fileName, "r") as f:
        for l in f.readlines():
            l = l.strip()

            count = l.count(" ")
            if count < 2:
                print("", file=stderr)
                print("Failed to generate ELF map info:", file=stderr)
                print("    Invalid line: '%s', expected 2 fields, got %d" % (l, count), file=stderr)
                print("", file=stderr)
                exit(1)

            (addr, type, name) = l.split(" ", 2)
            entries.append(KAllSymsEntry(addr, name, demangle(name)))

    return entries


if __name__ == "__main__":
    main()
