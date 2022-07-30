// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "lib/containers.h"

void _list_prepend(list_node_t *head, list_node_t *new_head)
{
    if (head == NULL || new_head == NULL)
        return;

    while (head->prev != NULL)
        head = head->prev;

    new_head->prev = NULL;
    new_head->next = head;
    head->prev = new_head;
}

void _list_append(list_node_t *head, list_node_t *new_tail)
{
    if (head == NULL || new_tail == NULL)
        return;

    while (head->next != NULL)
        head = head->next;

    head->next = new_tail;
    new_tail->prev = head;
    new_tail->next = NULL;
}

void _list_insert_after(list_node_t *element, list_node_t *node)
{
    if (element == NULL || node == NULL)
        return;

    node->prev = element;
    node->next = element->next;
    if (element->next != NULL)
        element->next->prev = node;
    element->next = node;
}

void _list_insert_before(list_node_t *element, list_node_t *node)
{
    if (element == NULL || node == NULL)
        return;

    node->next = element;
    node->prev = element->prev;
    if (element->prev != NULL)
        element->prev->next = node;
    element->prev = node;
}
