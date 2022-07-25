// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "attributes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef signed long long int s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef long double f80;

typedef u8 uchar;
typedef s8 schar;

static_assert(__SIZEOF_POINTER__ == sizeof(void *), "uintptr_t is not the same size as a pointer");
