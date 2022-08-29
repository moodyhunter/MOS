#!/bin/python

"""
#define SYSCALL_file_stat 2
#define MOS_SYSCALL_2(X)  X(file_stat, bool, (const char *, file), (file_stat_t *, out))
"""


import json
import os
from sys import argv

GEN_TYPE_TYPEDEF = "gen-typedef"
GEN_TYPE_DECL = "gen-decl"
GEN_TYPE_DISPATCHER = "gen-dispatcher"
GEN_TYPE_NUMBER_HEADER = "gen-number"

outfile = None
prefix = ""


def gen(str):
    outfile.write(str + "\n")


def main():
    global outfile
    global prefix

    if len(argv) != 5 or argv[2] not in [GEN_TYPE_TYPEDEF, GEN_TYPE_DECL, GEN_TYPE_DISPATCHER, GEN_TYPE_NUMBER_HEADER]:
        print("Usage:")
        print("  gen_syscall.py <prefix> COMMAND <syscall-json> <output-file>")
        print("")
        print("  COMMAND:")
        print("    %s: generate typedefs" % GEN_TYPE_TYPEDEF)
        print("    %s: generate declarations" % GEN_TYPE_DECL)
        print("    %s: generate dispatcher" % GEN_TYPE_DISPATCHER)
        print("    %s: generate syscall number header" % GEN_TYPE_NUMBER_HEADER)
        exit(1)

    prefix = argv[1]
    gen_type = argv[2]
    input_json = argv[3]
    output = argv[4]

    f = open(input_json, "r")
    j = json.load(f)

    if output == "-":
        outfile = os.fdopen(os.dup(1), "w")
    else:
        outfile = open(output, "w")

    gen("// SPDX-License-Identifier: GPL-3.0-or-later")
    gen("")
    gen("#pragma once")
    gen("")

    if gen_type != GEN_TYPE_NUMBER_HEADER:
        for inc in j["includes"]:
            gen("#include \"%s\"" % inc)
        gen("")

    if gen_type == GEN_TYPE_DISPATCHER:
        gen_dispatcher(j)
        exit(0)

    for e in j["syscalls"]:
        if gen_type == GEN_TYPE_TYPEDEF:
            gen_typedef(e)
        elif gen_type == GEN_TYPE_DECL:
            gen_decl(e)
        elif gen_type == GEN_TYPE_NUMBER_HEADER:
            gen_number_header(e)
        else:
            print("Unknown gen_type: %s" % gen_type)
            exit(1)
        gen("")

    if gen_type == GEN_TYPE_DECL:
        gen("#define define_%s(name) %s_##name" % (prefix, prefix))


def get_syscall_argdecls(e):
    s = []
    # fd_t fd, void *buffer, size_t size, size_t offset
    for a in e["arguments"]:
        spacer = "" if a["type"].endswith("*") else " "
        s.append(a["type"] + spacer + a["arg"])
    if len(s) == 0:
        return "void"
    return ", ".join(s)


def gen_typedef(e):
    gen("// syscall%d #%d: %s" % (len(e["arguments"]), e["number"], e["name"]))
    gen("typedef %s (*%s_%s_t)(%s)" % (prefix, e["return"], e["name"], get_syscall_argdecls(e)))


def gen_decl(e):
    gen("// %s%d #%d: %s" % (prefix, len(e["arguments"]), e["number"], e["name"]))
    gen("%s %s_%s(%s);" % (e["return"], prefix, e["name"], get_syscall_argdecls(e)))


def gen_dispatcher(j):
    gen("should_inline long dispatch_%s(const long number, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6, long arg7, long arg8)" % prefix)
    gen("{")
    for i in range(8):
        gen("    (void) arg%d;" % (i + 1))
    gen("")
    gen("    long ret = -1;")
    for e in j["syscalls"]:
        nargs = len(e["arguments"])
        gen("    extern %s %s_%s(%s);" % (e["return"], prefix, e["name"], get_syscall_argdecls(e)))
        gen("    if (number == %d)" % e["number"])
        gen("        ret = (long) %s_%s(%s);" % (prefix, e["name"], ", ".join(["(%s) arg%d" % (e["arguments"][i]["type"], i + 1) for i in range(nargs)])))
        gen("")

    gen("    return ret;")
    gen("}")


def gen_number_header(e):
    gen("#define SYSCALL_%s %d" % (e["name"], e["number"]))
    gen("#define SYSCALL_NAME_%d %s" % (e["number"], e["name"]))


if __name__ == "__main__":
    main()
