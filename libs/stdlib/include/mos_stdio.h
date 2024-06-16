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

typedef struct _FILE FILE;
MOSAPI FILE *stdin;
MOSAPI FILE *stdout;
MOSAPI FILE *stderr;

// C standard says they are macros, make them happy.
#define stdin  stdin
#define stdout stdout
#define stderr stderr

MOSAPI int __printf(1, 2) printf(const char *__restrict format, ...);
MOSAPI int __printf(2, 3) fprintf(FILE *__restrict file, const char *__restrict format, ...);
MOSAPI int __printf(2, 3) dprintf(int fd, const char *__restrict format, ...);
MOSAPI int vprintf(const char *__restrict format, va_list ap);
MOSAPI int vfprintf(FILE *__restrict file, const char *__restrict format, va_list ap);
MOSAPI int vdprintf(int fd, const char *__restrict format, va_list ap);

MOSAPI int getchar(void);
MOSAPI int putchar(int c);
MOSAPI int puts(const char *s);

MOSAPI int fputs(const char *__restrict s, FILE *__restrict file);
MOSAPI int fputc(int c, FILE *file);
MOSAPI int fgetc(FILE *file);

size_t fread(void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream);
size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream);
int fseek(FILE *stream, long offset, io_seek_whence_t whence);
off_t ftell(FILE *stream);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
#endif

/** @} */
