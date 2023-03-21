// SPDX-License-Identifier: GPL-3.0-or-later
#include "libuserspace.h"

#include <memory.h>
#include <mos/lib/structures/list.h>

static list_head atexit_funcs = LIST_HEAD_INIT(atexit_funcs);

typedef void (*destructor_t)(void *);

typedef struct
{
    as_linked_list;
    destructor_t destructor_func;
    void *obj_ptr;
    void *dso_handle;
} atexit_func_entry_t;

MOSAPI int __cxa_atexit(destructor_t f, void *objptr, void *dso)
{
    atexit_func_entry_t *node = calloc(sizeof(atexit_func_entry_t), 1);
    if (!node)
        return -1; // TODO: Set errno to ENOMEM

    node->destructor_func = f;
    node->obj_ptr = objptr;
    node->dso_handle = dso;

    list_node_append(&atexit_funcs, list_node(node));

    return 0;
}

MOSAPI void __cxa_finalize(destructor_t f)
{
    list_foreach(atexit_func_entry_t, node, atexit_funcs)
    {
        if (f == NULL || f == node->destructor_func)
        {
            node->destructor_func(node->obj_ptr);
            list_remove(node);
            free(node);
        }
    }
}

int atexit(void (*func)(void))
{
    return __cxa_atexit((destructor_t) func, NULL, NULL);
}
