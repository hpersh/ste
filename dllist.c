#include <assert.h>

#ifndef NDEBUG
#define DEBUG_ASSERT(x)  assert(x)
#else
#define DEBUG_ASSERT(x)
#endif

#include "dllist.h"

struct dllist *dllist_insert(struct dllist *nd, struct dllist *before)
{
    struct dllist *p = before->prev;

    nd->prev = p;
    nd->next = before;

    return (p->next = before->prev = nd);
}


struct dllist *dllist_erase(struct dllist *nd)
{
    struct dllist *p = nd->prev, *q = nd->next;

    p->next = q;
    q->prev = p;
#ifndef NDEBUG
    nd->prev = nd->next = 0;
#endif

    return (nd);
}
