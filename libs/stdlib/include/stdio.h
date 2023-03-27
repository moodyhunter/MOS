// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/mos_global.h>
#include <mos/moslib_global.h>
#include <stdarg.h>
#include <stddef.h>

/**
 * @defgroup libs_stdio libs.Stdio
 * @ingroup libs
 * @brief Standard input/output functions.
 * @{
 */

// defined in stdio.c & stdio_impl.c
MOSAPI int __printf(2, 3) sprintf(char *__restrict str, const char *__restrict format, ...);
MOSAPI int __printf(3, 4) snprintf(char *__restrict str, size_t size, const char *__restrict format, ...);
MOSAPI int vsprintf(char *__restrict str, const char *__restrict format, va_list ap);
MOSAPI int vsnprintf(char *__restrict buf, size_t size, const char *__restrict format, va_list args);

#ifndef __MOS_KERNEL__ // for userspace only

struct FILE;
MOSAPI struct FILE *stdin;
MOSAPI struct FILE *stdout;
MOSAPI struct FILE *stderr;

// C standard says they are macros, make them happy.
#define stdin  stdin
#define stdout stdout
#define stderr stderr

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

MOSAPI int __printf(1, 2) printf(const char *__restrict format, ...);
MOSAPI int __printf(2, 3) fprintf(struct FILE *__restrict file, const char *__restrict format, ...);
MOSAPI int __printf(2, 3) dprintf(int fd, const char *__restrict format, ...);
MOSAPI int vprintf(const char *__restrict format, va_list ap);
MOSAPI int vfprintf(struct FILE *__restrict file, const char *__restrict format, va_list ap);
MOSAPI int vdprintf(int fd, const char *__restrict format, va_list ap);

MOSAPI int getchar(void);
MOSAPI int putchar(int c);
MOSAPI int puts(const char *s);

MOSAPI int fputs(const char *__restrict s, struct FILE *__restrict file);
MOSAPI int fputc(int c, struct FILE *file);
MOSAPI int fgetc(struct FILE *file);

size_t fread(void *__restrict ptr, size_t size, size_t nmemb, struct FILE *__restrict stream);
size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, struct FILE *__restrict stream);
#endif

/** @} */
