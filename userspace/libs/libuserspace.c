// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"

#include "lib/liballoc.h"
#include "lib/memory.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "mos/syscall/usermode.h"

#include <stdarg.h>

typedef struct thread_start_args
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

u64 __stack_chk_guard = 0xdeadbeefdeadbeef;

noreturn void __stack_chk_fail(void)
{
    syscall_exit(-1);
    while (1)
        ;
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

static void invoke_init(void)
{
    typedef void (*func_ptr)(void);
    extern char __init_array_start, __init_array_end;
    for (func_ptr *func = (void *) &__init_array_start; func != (void *) &__init_array_end; func++)
        if (func)
            (*func)();
}

static void __attribute__((constructor)) __liballoc_userspace_init(void)
{
    liballoc_init();
}

void *malloc(size_t size)
{
    return liballoc_malloc(size);
}

void free(void *ptr)
{
    liballoc_free(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
    return liballoc_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    return liballoc_realloc(ptr, size);
}

void _start(size_t argc, char **argv)
{
    extern int main(int argc, char **argv);
    extern void __cxa_finalize(void *d);

    invoke_init();
    int r = main(argc, argv);
    __cxa_finalize(NULL);
    syscall_exit(r);
}

static void thread_start(void *_arg)
{
    thread_entry_t entry = ((thread_start_args_t *) _arg)->entry;
    void *entry_arg = ((thread_start_args_t *) _arg)->arg;
    free(_arg);
    entry(entry_arg);
    syscall_thread_exit();
}

int vdprintf(int fd, const char *fmt, va_list ap)
{
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    syscall_io_write(fd, buf, len, 0);
    return len;
}

int dprintf(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(fd, fmt, ap);
    va_end(ap);
    return size;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stdout, fmt, ap);
    va_end(ap);
    return size;
}

void fatal_abort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(stderr, fmt, ap);
    va_end(ap);
    syscall_exit(-1);
    while (1)
        ;
}

void *liballoc_alloc_page(size_t npages)
{
    uintptr_t new_top = syscall_heap_control(HEAP_GROW_PAGES, npages);
    if (new_top == 0)
        return NULL;

    return (void *) (new_top - npages * MOS_PAGE_SIZE);
}

bool liballoc_free_page(void *vptr, size_t npages)
{
    MOS_UNUSED(npages);
    MOS_UNUSED(vptr);
    dprintf(stderr, "liballoc_free_page not implemented\n");
    return false;
}

tid_t start_thread(const char *name, thread_entry_t entry, void *arg)
{
    thread_start_args_t *thread_start_args = malloc(sizeof(thread_start_args_t));
    thread_start_args->entry = entry;
    thread_start_args->arg = arg;
    return syscall_create_thread(name, thread_start, thread_start_args);
}
