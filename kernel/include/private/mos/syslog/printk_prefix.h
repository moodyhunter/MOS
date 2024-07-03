// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

#include <mos/mos_global.h>

#if MOS_CONFIG(MOS_PRINTK_WITH_TIMESTAMP)
#define _lprintk_timestamp_fmt "%-16llu | "
#define _lprintk_timestamp_arg platform_get_timestamp()
#else
#define _lprintk_timestamp_fmt
#define _lprintk_timestamp_arg
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_DATETIME)
#define _lprintk_datetime_fmt "%s | "
#define _lprintk_datetime_arg (const char *) platform_get_datetime_str()
#else
#define _lprintk_datetime_fmt
#define _lprintk_datetime_arg
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_CPU_ID)
#define _lprintk_cpuid_fmt "cpu %2d | "
#define _lprintk_cpuid_arg platform_current_cpu_id()
#else
#define _lprintk_cpuid_fmt
#define _lprintk_cpuid_arg
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_FILENAME)
#define _lprintk_filename_fmt "%-20s | "
#define _lprintk_filename_arg MOS_FILE_LOCATION
#else
#define _lprintk_filename_fmt
#define _lprintk_filename_arg
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_THREAD_ID)
#define _lprintk_threadid_fmt "%pt\t| "
#define _lprintk_threadid_arg ((void *) current_thread)
#else
#define _lprintk_threadid_fmt
#define _lprintk_threadid_arg
#endif

// don't use MOS_CONFIG(MOS_PRINTK_HAS_SOME_PREFIX) here as it may not be defined
#if MOS_PRINTK_HAS_SOME_PREFIX
#define __add_comma(...) __VA_OPT__(, (__VA_ARGS__))
// clang-format off
#define _lprintk_prefix_fmt             \
    _lprintk_timestamp_fmt              \
    _lprintk_datetime_fmt               \
    _lprintk_cpuid_fmt                  \
    _lprintk_filename_fmt               \
    _lprintk_threadid_fmt

#define _lprintk_prefix_arg                                                                                                                                              \
    __add_comma(_lprintk_timestamp_arg) \
    __add_comma(_lprintk_datetime_arg)  \
    __add_comma(_lprintk_cpuid_arg)     \
    __add_comma(_lprintk_filename_arg)  \
    __add_comma(_lprintk_threadid_arg)
// clang-format on
#else
#define _lprintk_prefix_fmt
#define _lprintk_prefix_arg // empty so that the comma after fmt is not included
#endif

// ! comma after fmt is included in '_lprintk_prefix_arg', careful when changing this macro
#define lprintk_wrapper(level, fmt, ...) lprintk(level, "\r\n" _lprintk_prefix_fmt fmt _lprintk_prefix_arg, ##__VA_ARGS__)
