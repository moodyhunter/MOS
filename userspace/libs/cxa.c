// SPDX-License-Identifier: GPL-3.0-or-later
#include "lib/memory.h"
#include "lib/structures/list.h"
#include "libuserspace.h"

static list_node_t atexit_funcs = LIST_HEAD_INIT(atexit_funcs);
void *__dso_handle = 0;

typedef void (*destructor_t)(void *);

typedef struct
{
    as_linked_list;
    destructor_t destructor_func;
    void *obj_ptr;
    void *dso_handle;
} atexit_func_entry_t;

__mosapi int __cxa_atexit(destructor_t f, void *objptr, void *dso)
{
    atexit_func_entry_t *node = malloc(sizeof(atexit_func_entry_t));
    if (!node)
        return -1; // TODO: Set errno to ENOMEM

    linked_list_init(list_node(node));
    node->destructor_func = f;
    node->obj_ptr = objptr;
    node->dso_handle = dso;

    list_node_append(&atexit_funcs, list_node(node));

    return 0;
}

__mosapi void __cxa_finalize(destructor_t f)
{
    if (f == NULL)
    {
        while (!list_is_empty(&atexit_funcs))
        {
            atexit_func_entry_t *next = list_entry(atexit_funcs.next, atexit_func_entry_t);
            next->destructor_func(next->obj_ptr);
            list_remove(next);
            free(next);
        }

        return;
    }

    while (!list_is_empty(&atexit_funcs))
    {
        // call all destructors on the list in reverse order of their registration
        atexit_func_entry_t *last = list_entry(atexit_funcs.prev, atexit_func_entry_t);
        if (last->destructor_func == f)
        {
            last->destructor_func(last->obj_ptr);
            list_remove(last);
            free(last);
        }

        return;
    }
}
