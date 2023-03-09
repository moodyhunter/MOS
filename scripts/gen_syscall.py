#!/usr/bin/env python

import io
import json
import os
from sys import argv

GEN_TYPE_DECL = "gen-decl"
GEN_TYPE_DISPATCHER = "gen-dispatcher"
GEN_TYPE_NUMBER_HEADER = "gen-number"
GEN_TYPE_USERMODE = "gen-usermode"

MAX_SYSCALL_NARGS = 6

outfile: io.TextIOBase = None


class Scope:
    def __init__(self):
        self._scope = 0

    def __enter__(self):
        self._scope += 1

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._scope -= 1


scope = Scope()


def gen(str):
    for _ in range(scope._scope):
        outfile.write("    ")
    outfile.write(str + "\n")


def forall_syscalls_do_gen(j, gen_func):
    for e in j["syscalls"]:
        gen_func(e)


def insert_includes(j):
    for inc in j["includes"]:
        gen("#include \"%s\"" % inc)
    gen("")


def main():
    global outfile

    if len(argv) != 4 or argv[1] not in [GEN_TYPE_DECL, GEN_TYPE_DISPATCHER, GEN_TYPE_NUMBER_HEADER, GEN_TYPE_USERMODE]:
        print("Usage:")
        print("  gen_syscall.py COMMAND <syscall-json> <output-file>")
        print("")
        print("  COMMAND:")
        print("    %s: generate declarations" % GEN_TYPE_DECL)
        print("    %s: generate dispatcher" % GEN_TYPE_DISPATCHER)
        print("    %s: generate syscall number header" % GEN_TYPE_NUMBER_HEADER)
        print("    %s: generate usermode invoker" % GEN_TYPE_USERMODE)
        exit(1)

    gen_type = argv[1]
    input_json = argv[2]
    output = argv[3]

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

    # sort the includes
    j["includes"].sort()

    if gen_type == GEN_TYPE_USERMODE:
        insert_includes(j)
        # also include the 'mos/platform_syscall.h' header
        gen("// platform syscall header")
        gen('#include "mos/platform_syscall.h"')
        gen("")
        forall_syscalls_do_gen(j, gen_single_usermode_wrapper)

    elif gen_type == GEN_TYPE_DISPATCHER:
        insert_includes(j)
        gen("// syscall implementation declarations and syscall numbers")
        gen('#include "mos/syscall/decl.h"')
        gen('#include "mos/syscall/number.h"')
        gen("")
        gen("should_inline reg_t dispatch_syscall(const reg_t number, %s)" % (", ".join(["reg_t arg%d" % (i + 1) for i in range(MAX_SYSCALL_NARGS)])))
        gen("{")
        with scope:
            for i in range(MAX_SYSCALL_NARGS):
                gen("MOS_UNUSED(arg%d);" % (i + 1))
            gen("")
            gen("reg_t ret = 0;")
            gen("")
            gen("switch (number)")
            gen("{")
            with scope:
                forall_syscalls_do_gen(j, gen_single_dispatcher)
            gen("}")
            gen("")
            gen("return ret;")
        gen("}")

    elif gen_type == GEN_TYPE_DECL:
        insert_includes(j)
        forall_syscalls_do_gen(j, gen_single_kernel_impl_decl)
        gen("#define define_syscall(name) impl_syscall_##name")

    elif gen_type == GEN_TYPE_NUMBER_HEADER:
        forall_syscalls_do_gen(j, gen_single_number_define)
    else:
        print("Unknown gen_type: %s" % gen_type)
        exit(1)

    exit(0)


def syscall_args(e):
    s = []
    # fd_t fd, void *buffer, size_t size, size_t offset
    for a in e["arguments"]:
        spacer = "" if a["type"].endswith("*") else " "
        s.append(a["type"] + spacer + a["arg"])
    if len(s) == 0:
        return "void"
    return ", ".join(s)


def syscall_is_noreturn(e):
    return e["return"] is None


def syscall_return(e) -> str:
    return "void" if syscall_is_noreturn(e) else e["return"]


def syscall_has_return(e):
    return (not syscall_is_noreturn(e)) and (syscall_return(e) != "void")


def syscall_attr(e):
    return "noreturn " if syscall_is_noreturn(e) else ""


def syscall_name(e):
    return "syscall_" + e["name"]


def gen_single_kernel_impl_decl(e):
    gen("%s%s %s(%s);" % (syscall_attr(e), syscall_return(e), "impl_" + syscall_name(e), syscall_args(e)))


def gen_single_dispatcher(e):
    nargs = len(e["arguments"])
    syscall_arg_casted = ", ".join(["(%s) arg%d" % (e["arguments"][i]["type"], i + 1) for i in range(nargs)])

    gen("case SYSCALL_%s:" % e["name"])
    gen("{")
    with scope:
        gen("%s%s(%s);" % ("ret = (reg_t) " if syscall_has_return(e) else "", "impl_" + syscall_name(e), syscall_arg_casted))
        gen("break;")
    gen("}")


def gen_single_number_define(e):
    gen("#define SYSCALL_%s %d" % (e["name"], e["number"]))
    gen("#define SYSCALL_NAME_%d %s" % (e["number"], e["name"]))


def gen_single_usermode_wrapper(e):
    syscall_nargs = len(e["arguments"])
    syscall_conv_arg_to_reg_type = ", ".join([str(e["number"])] + ["(reg_t) %s" % arg["arg"] for arg in e["arguments"]])

    comments = e["comments"] if "comments" in e else []
    if len(comments) > 0:
        gen("/**")
        gen(" * %s" % e["name"])

    for comment in e["comments"] if "comments" in e else []:
        gen(" * %s" % comment)

    if len(comments) > 0:
        gen(" */")

    gen("should_inline %s %s(%s)" % (syscall_return(e), syscall_name(e), syscall_args(e)))
    gen("{")
    with scope:
        gen("%s%splatform_syscall%d(%s);" % ("return " if syscall_has_return(e) else "",
                                             "(" + syscall_return(e) + ") " if syscall_return(e) != "void" else "",
                                             syscall_nargs,
                                             syscall_conv_arg_to_reg_type))
    gen("}")


if __name__ == "__main__":
    main()
