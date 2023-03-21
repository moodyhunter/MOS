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
        gen("")


def insert_includes(j):
    for inc in j["includes"]:
        gen("#include <%s>" % inc)
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
        gen('#include <mos/platform_syscall.h>')
        gen("")
        gen("#ifdef __MOS_KERNEL__")
        gen("#error \"This file should not be included in the kernel!\"")
        gen("#endif")
        gen("")
        forall_syscalls_do_gen(j, gen_single_usermode_wrapper)

    elif gen_type == GEN_TYPE_DISPATCHER:
        insert_includes(j)
        gen("// syscall implementation declarations and syscall numbers")
        gen('#include <mos/syscall/decl.h>')
        gen('#include <mos/syscall/number.h>')
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
                forall_syscalls_do_gen(j, gen_single_dispatcher_case)
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


def syscall_has_return_value(e):
    return (not syscall_is_noreturn(e)) and (e["return"] != "void")


def syscall_name_with_prefix(e):
    return "syscall_" + e["name"]


def syscall_format_return_type(e) -> str:
    if syscall_is_noreturn(e):
        return "void" + " "
    elif e["return"].endswith("*"):
        return e["return"]  # make * stick to the type
    else:
        return e["return"] + " "


def gen_single_kernel_impl_decl(e):
    gen("%s%s%s(%s);" % (
        "noreturn " if syscall_is_noreturn(e) else "",
        syscall_format_return_type(e),
        "impl_" + syscall_name_with_prefix(e),
        syscall_args(e)
    ))


def gen_single_dispatcher_case(e):
    nargs = len(e["arguments"])
    syscall_arg_casted = ", ".join(["(%s) arg%d" % (e["arguments"][i]["type"], i + 1) for i in range(nargs)])
    retval_assign = "ret = (reg_t) " if syscall_has_return_value(e) else ""

    gen("case SYSCALL_%s:" % e["name"])
    gen("{")
    with scope:
        gen("%simpl_%s(%s);" % (retval_assign, syscall_name_with_prefix(e), syscall_arg_casted))
        gen("break;")
    gen("}")


def gen_single_number_define(e):
    gen("#define SYSCALL_%s %d" % (e["name"], e["number"]))
    gen("#define SYSCALL_NAME_%d %s" % (e["number"], e["name"]))


def gen_single_usermode_wrapper(e):
    syscall_nargs = len(e["arguments"])
    syscall_conv_arg_to_reg_type = ", ".join([str(e["number"])] + ["(reg_t) %s" % arg["arg"] for arg in e["arguments"]])
    comments = e["comments"] if "comments" in e else []
    return_stmt = "return (" + e["return"] + ") " if syscall_has_return_value(e) else ""

    if len(comments) > 0:
        gen("/**")
        gen(" * %s" % e["name"])
        for comment in comments:
            gen(" * %s" % comment)
        gen(" */")

    gen("should_inline %s%s(%s)" % (syscall_format_return_type(e), syscall_name_with_prefix(e), syscall_args(e)))
    gen("{")
    with scope:
        gen("%splatform_syscall%d(%s);" % (return_stmt,
                                           syscall_nargs,
                                           syscall_conv_arg_to_reg_type))
    gen("}")


if __name__ == "__main__":
    main()
