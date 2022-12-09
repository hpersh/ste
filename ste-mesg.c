#include "dllist,h"
#include "ste.h"
#include "ste-mesg.h"
#include "ste-mesg-config.h"

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define FIELD_OFS(s, f)                   ((uintptr_t) &((s *) 0)->f)
#define FIELD_PTR_TO_STRUCT_PTR(p, s, f)  ((s *)((unsigned char *)(p) - FIELD_OFS(s, f)))

struct _mesg {
    ste_mesg_data_t mesg_data;
    struct dllist   list_node[1];
};



struct mesg_buf_pool {
    struct dllist free[1];
};

static struct mesg_buf_pool pool_tbl[STE_MESG_NUM_POOLS];

static void pools_init(void)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(pool_tbl); ++i) {
        dllist_init(pool_tbl[i].free);
        unsigned char *p;
        size_t rem;
        for (p = ste_mesg_pool_cfg[i].addr, rem = ste_mesg_pool_cfg[i].size; rem >= sizeof(struct mesg_buf); rem -= sizeof(struct mesg_buf), p += sizeof(struct mesg_buf)) {
            dllist_insert(((struct mesg_buf *) p)->list_node, dllist_end(pool_tbl[i].free));
        }
    }
}


static inline struct mesg_buf_pool *mesg_buf_pool_find(unsigned id)
{
    return (id >= ARRAY_SIZE(mesg_buf_pool_tbl) ? 0 : &mesg_buf_pool_tbl[id]);
}

struct mbox {
    struct dllist queue[1];
    unsigned      task_id;
    unsigned      signal;
};

static struct mbox mbox_tbl[STE_MESG_NUM_MBOXES];

static void mboxes_init(void)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(mbox_tbl); ++i) {
        dllist_init(mbox_tbl[i].queue);
    }
}


static inline struct mbox *mbox_find(unsigned id)
{
    return (id >= ARRAY_SIZE(mbox_tbl) ? 0 : &mbox_tbl[id]);
}


int ste_mesg_mbox_init(unsigned mbox_id, unsigned task_id, unsigned signal)
{
    struct mbox *m = mbox_find(mbox_id);
    if (m == 0)  return (STE_ERR_BAD_ID);
    
    m->task_id = task_id;
    m->signal  = signal;

    return (STE_OK);
}


int ste_mesg_send(unsigned mbox_id, unsigned pool_id, ste_mesg_t mesg, unsigned flags)
{
    struct mesg_buf_pool *pool = mesg_buf_pool_find(pool_id);
    if (pool == 0)  return (STE_ERR_BAD_ID);
    struct mbox *mbox = mbox_find(mbox_id);
    if (mbox == 0)  return (STE_ERR_BAD_ID);

    for (;;) {
        struct dllist *p = dllist_first(pool->free);
        if (p != dllist_end(pool->free))  break;
        if (flags & STE_MESG_SEND_FLAG_NOWAIT)  return (STE_ERR_MESG_POOL_EMPTY);

        ste_task_wait();
    }
    dllist_erase(p);

    FIELD_PTR_TO_STRUCT_PTR(p, struct mesg_buf, list_node)->mesg = mesg;
    dllist_insert(p, dllist_end(mbox->queue));

    ste_task_signal(mbox->task_id, mbox->signal);

    return (STE_OK);
}


int str_mesg_recv(unsigned mbox_id, ste_mesg_t *mesg)
{
    struct mbox *mbox = mbox_find(mbox_id);
    if (mbox == 0)  return (STE_ERR_BAD_ID);

    struct dllist *p = dllist_first(mbox->queue);
    if (p == dllist_end(mbox->queue))  return (STE_MESG_MBOX_EMPTY);

    struct mesg_buf *m = FIELD_PTR_TO_STRUCT_PTR(p, struct mesg_buf, list_node);
    *mesg = m->mesg;

    dllist_erase(p);
    dllist_inster(p, dllist_end(m->pool->free));

    return (STE_OK);
}


    
