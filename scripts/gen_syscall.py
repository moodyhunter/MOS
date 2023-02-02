#!/bin/python

"""
#define SYSCALL_file_stat 2
#define MOS_SYSCALL_2(X)  X(file_stat, bool, (const char *, file), (file_stat_t *, out))
"""


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

scope = 0


def enter_scope():
    global scope
    scope += 1


def leave_scope():
    global scope
    scope -= 1


def gen(str):
    for _ in range(scope):
        outfile.write("    ")
    outfile.write(str + "\n")


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

    if gen_type != GEN_TYPE_NUMBER_HEADER:
        for inc in j["includes"]:
            gen("#include \"%s\"" % inc)
        gen("")

        if gen_type == GEN_TYPE_USERMODE:
            # also include the 'mos/platform_syscall.h' header
            gen("// platform syscall header")
            gen("#include \"mos/platform_syscall.h\"")
            gen("")
            pass

    if gen_type == GEN_TYPE_DISPATCHER:
        gen_dispatcher(j)
        exit(0)

    for e in j["syscalls"]:
        if gen_type == GEN_TYPE_DECL:
            gen_kernel_impl_decl(e)
        elif gen_type == GEN_TYPE_NUMBER_HEADER:
            gen_number_header(e)
        elif gen_type == GEN_TYPE_USERMODE:
            gen_usermode(e)
        else:
            print("Unknown gen_type: %s" % gen_type)
            exit(1)
        gen("")

    if gen_type == GEN_TYPE_DECL:
        gen("#define define_syscall(name) impl_syscall_##name")


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


def gen_kernel_impl_decl(e):
    gen("%s%s %s(%s);" % (syscall_attr(e), syscall_return(e), "impl_" + syscall_name(e), syscall_args(e)))


def gen_dispatcher(j):
    gen("should_inline long dispatch_syscall(const long number, %s)" % (", ".join(["long arg%d" % (i + 1) for i in range(MAX_SYSCALL_NARGS)])))
    gen("{")
    enter_scope()
    for i in range(MAX_SYSCALL_NARGS):
        gen("(void) arg%d;" % (i + 1))
    gen("")
    gen("long ret = 0;")
    for e in j["syscalls"]:
        nargs = len(e["arguments"])
        syscall_arg_casted = ", ".join(["(%s) arg%d" % (e["arguments"][i]["type"], i + 1) for i in range(nargs)])

        gen("extern %s%s %s(%s);" % (syscall_attr(e), syscall_return(e), "impl_" + syscall_name(e), syscall_args(e)))
        gen("if (number == %d)" % e["number"])
        enter_scope()
        gen("%s%s(%s);" % ("ret = (long) " if syscall_has_return(e) else "", "impl_" + syscall_name(e), syscall_arg_casted))
        leave_scope()
        gen("")

    gen("return ret;")
    leave_scope()
    gen("}")


def gen_number_header(e):
    gen("#define SYSCALL_%s %d" % (e["name"], e["number"]))
    gen("#define SYSCALL_NAME_%d %s" % (e["number"], e["name"]))


def gen_usermode(e):
    syscall_nargs = len(e["arguments"])
    syscall_conv_arg_to_long = ", ".join([str(e["number"])] + ["(long) %s" % arg["arg"] for arg in e["arguments"]])

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
    enter_scope()
    gen("%s%splatform_syscall%d(%s);" % ("return " if syscall_has_return(e) else "",
                                         "(" + syscall_return(e) + ") " if syscall_return(e) != "void" else "",
                                         syscall_nargs,
                                         syscall_conv_arg_to_long))
    leave_scope()
    gen("}")


if __name__ == "__main__":
    main()
