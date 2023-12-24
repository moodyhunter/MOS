// SPDX-License-Identifier: GPL-3.0-or-later

#include <liballoc.h>
#include <mos/syscall/usermode.h>
#include <mos_stdlib.h>

void *liballoc_alloc_page(size_t npages)
{
    ptr_t new_top = syscall_heap_control(HEAP_GROW_PAGES, npages);
    if (new_top == 0)
        return NULL;

    return (void *) (new_top - npages * MOS_PAGE_SIZE);
}

bool liballoc_free_page(void *vptr, size_t npages)
{
    syscall_munmap(vptr, npages * MOS_PAGE_SIZE);
    return true;
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

void exit(int status)
{
    atexit(NULL);
    syscall_exit(status);
}
