#ifndef LIST_H
#define LIST_H

#include <stddef.h>

typedef struct {
    list_node *prev;
    list_node *next;
} list_node;

#define list_new(name) { &(name), &(name) }
#define list_entry(ptr, type, member) ((type *) ((char *) (ptr) - (char *) &((type *) 0)->member))

void list_insert(list_node *dest, list_node *node)
{
    if (dest == NULL || node == NULL) return;
    if (dest->next != NULL) dest->next->prev = node;
    node->next = dest->next;
    dest->next = node;
    node->prev = dest;
}

void list_remove(list_node *node)
{
    if (node->prev != NULL) node->prev->next = node->next;
    if (node->next != NULL) node->next->prev = node->prev;
}

#define list_for(ptr, head) \
    for (ptr = (head)->next; ptr != (head); ptr = ptr->next)

#endif
