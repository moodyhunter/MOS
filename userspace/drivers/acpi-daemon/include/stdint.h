// SPDX-License-Identifier: GPL-3.0-or-later
// WARNING: vendored stdint.h, do not use directly

#pragma once

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;

typedef __UINTPTR_TYPE__ uintptr_t;

#define UINT64_C(x) x##ULL
