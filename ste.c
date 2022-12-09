#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifndef NDEBUG
#define DEBUG_ASSERT(x)  assert(x)
#else
#define DEBUG_ASSERT(x)
#endif

#include "dllist.h"
#include "ste.h"

enum { false, true };

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define FIELD_OFS(s, f)                   ((uintptr_t) &((s *) 0)->f)
#define FIELD_PTR_TO_STRUCT_PTR(p, s, f)  ((s *)((unsigned char *)(p) - FIELD_OFS(s, f)))

#define INTR_MASK_BEGIN                                                 \
    {                                                                   \
    unsigned __old = cpu_intr_disable(STE_KERNEL_INTR_DISABLE_LVL);

#define INTR_MASK_END                           \
    cpu_intr_restore(__old);                    \
    }


/* Task control block */
struct tcb {
    struct ste_task base[1];
    struct dllist   list_node[1];
};

/* Storage for task control blocks */
static struct tcb task_tbl[STE_MAX_TASKS];

/* Currently running task */
static struct tcb *task_current;

/* External, CPU-dependent functions, to operate on task context */
void *ste_task_cntxt_init(void *stack_top, void *entry, void *arg, void *exit);
void *ste_task_cntxt_save(void **sp);
void ste_task_cntxt_restore(void *sp);

static struct dllist rdy_pri_tbl[1 << STE_TASK_PRI_BITS];

static void rdyq_init(void)
{
    memset(task_tbl, 0, sizeof(task_tbl));
    struct dllist *p;
    unsigned n;
    for (p = rdy_pri_tbl, n = ARRAY_SIZE(rdy_pri_tbl); n > 0; --n, ++p) {
        dllist_init(p);
    }
}


static inline void rdyq_insert_unsafe(struct tcb *p)
{
    dllist_insert(p->list_node,
                  dllist_end(&rdy_pri_tbl[(unsigned) p->base->pri & (ARRAY_SIZE(rdy_pri_tbl) - 1)])
                  );
}


static inline void rdyq_delete_unsafe(struct tcb *p)
{
    dllist_erase(p->list_node);
}


static struct tcb *rdyq_pop_unsafe(void)
{
    unsigned n = ARRAY_SIZE(rdy_pri_tbl);
    unsigned i = n >> 1;
    for (; n > 0; --n, i = (i + 1) & (ARRAY_SIZE(rdy_pri_tbl) - 1)) {
        struct dllist *p = &rdy_pri_tbl[i];
        if (dllist_is_empty(p))  continue;
        
        struct dllist *q = dllist_first(p);
        dllist_erase(q);
        
        return (FIELD_PTR_TO_STRUCT_PTR(q, struct tcb, list_node));
    }
    
    return (0);
}

/* Resume the next task in the ready queue */

static void timers_run(void);

static void task_resume(void)
{
    struct ste_task *q = 0;

    for (;;) {
        /* See if any timers have expired */
        timers_run();

        INTR_MASK_BEGIN {
            task_current = rdyq_pop_unsafe();
            if (task_current != 0) {
                q = task_current->base;
                q->state = STE_TASK_STATE_RUNNING;
            }
        } INTR_MASK_END;
    
        if (task_current != 0) {
            q->last_scheduled = ticks_get();
            
            ste_task_cntxt_restore(q->sp);
        }

        /* No ready tasks => Wait for an interrupt*/
        
        cpu_intr_wait();
    }
}


static void task_select(void)
{
    struct ste_task *q = task_current->base;

    /* Record how much time used */
    q->total_time += ticks_get() - q->last_scheduled;

    /* Switch tasks */
    if (ste_task_cntxt_save(&q->sp) == 0)  return;
    task_resume();
}

/* Function called if a task ever does a top-level return */

static void task_exit(void)
{
    if (ste_task_exit_hook != 0)  (*ste_task_exit_hook)();

    task_current->base->state = STE_TASK_STATE_EXITED;

    task_select();
}

/* Create a task */

int ste_task_create(unsigned   id,
                    const char *name,
                    int        pri,
                    void       *stack,
                    unsigned   stack_size,
                    void       *entry,
                    void       *arg,
                    void       *data
                    )
{
    if (id >= ARRAY_SIZE(task_tbl))  return (STE_ERR_BAD_ID);
    if (pri < STE_TASK_PRI_MAX || pri > STE_TASK_PRI_MIN)  return (STE_ERR_BAD_PARAM);

    struct tcb *p = &task_tbl[id];
    struct ste_task *q = p->base;

    switch (q->state) {
    case STE_TASK_STATE_DEAD:
    case STE_TASK_STATE_EXITED:
        break;
    default:
        return (STE_ERR_CLOBBER);
    }
        
    memset(q, 0, sizeof(*q));
    q->name  = name;
    q->pri   = pri;
    q->stack = stack;
    q->stack_top = (unsigned char *) stack + stack_size;
    q->sp = ste_task_cntxt_init(q->stack_top, entry, arg, task_exit);
    q->data = data;
    
    INTR_MASK_BEGIN {
        q->state = STE_TASK_STATE_READY;
        rdyq_insert_unsafe(p);
    } INTR_MASK_END;
    
    return (STE_OK);
}


static struct tcb *task_id_to_ptr(unsigned id)
{
    if (id == STE_TASK_ID_SELF)  return (task_current);
    if (id >= ARRAY_SIZE(task_tbl))  return (0);
    return (&task_tbl[id]);
}

/* Cancel a task */

int ste_task_cancel(unsigned id)
{
    struct tcb *p = task_id_to_ptr(id);
    if (p == 0)  return (STE_ERR_BAD_ID);
    struct ste_task *q = p->base;
    bool yieldf = false;
    
    INTR_MASK_BEGIN {
        switch (q->state) {
        case STE_TASK_STATE_READY:
            rdyq_delete_unsafe(p);
            break;
        case STE_TASK_STATE_RUNNING:
            yieldf = true;
            break;
        default: ;
        }
        q->state = STE_TASK_STATE_DEAD;
    } INTR_MASK_END;

    if (yieldf)  task_select();
    
    return (STE_OK);
}

/* Send signals to a task */

static bool task_wait_satisfied(struct ste_task *q)
{
    ste_task_signals_t g = q->signals_got & q->signals_wait;
    return ((q->signals_wait_mode == STE_TASK_WAIT_MODE_OR && g != 0)
            || (q->signals_wait_mode == STE_TASK_WAIT_MODE_AND && g == q->signals_wait)
            );
}


static void _task_signal(struct tcb *p, ste_task_signals_t signals)
{
    struct ste_task *q = p->base;

    INTR_MASK_BEGIN {
        q->signals_got |= signals;

        if (q->state == STE_TASK_STATE_WAITING
            && task_wait_satisfied(q)
            ) {
            /* Signal wait condition satisfied => Mark task as ready */
            
            q->state = STE_TASK_STATE_READY;
            rdyq_insert_unsafe(p);
        }
    } INTR_MASK_END;
}

/* Send signals to a task */

int ste_task_signal(unsigned id, ste_task_signals_t signals)
{
    struct tcb *p = task_id_to_ptr(id);
    if (p == 0)  return (STE_ERR_BAD_ID);
    
    _task_signal(p, signals);

    return (STE_OK);
}

/* Return received signals */

ste_task_signals_t ste_task_signals_get(void)
{
    return (task_current->base->signals_got);
}

/* Clear given signals */

void ste_task_signals_clr(ste_task_signals_t signals)
{
    INTR_MASK_BEGIN {
        task_current->base->signals_got &= ~signals;
    } INTR_MASK_END;
}

/* Give up the CPU to the next task */

void ste_task_yield(void)
{
    INTR_MASK_BEGIN {
        task_current->base->state = STE_TASK_STATE_READY;
        rdyq_insert_unsafe(task_current);
    } INTR_MASK_END;
    
    task_select();
}

/* Wait for given signals */

void ste_task_wait(ste_task_signals_t signals_clr, ste_task_signals_t signals_wait, unsigned wait_mode)
{
    struct ste_task *q = task_current->base;
    bool yieldf = false;

    INTR_MASK_BEGIN {
        q->signals_wait      = signals_wait;
        q->signals_wait_mode = wait_mode;
        q->signals_got       &= ~signals_clr;

        if (!task_wait_satisfied(q)) {
            q->state = STE_TASK_STATE_WAITING;
            yieldf = true;
        }
    } INTR_MASK_END;

    if (yieldf)  task_select();
}


void *task_data_get(unsigned id)
{
    struct tcb *p = task_id_to_ptr(id);

    return (p == 0 ? 0 : p->base->data);
}

struct timercb {
    struct ste_timer base[1];
    struct dllist    list_node[1];
};

/* Storage for timer control blocks */

static struct timercb timer_tbl[STE_MAX_TIMERS];
static struct dllist timers_running[1];

static void timers_init(void)
{
    memset(timer_tbl, 0, sizeof(timer_tbl));
    dllist_init(timers_running);
}

/* Start a timer */

int ste_timer_start(unsigned id, const char *name, unsigned type, ste_ticks_t tmout, unsigned task_id, ste_task_signals_t signal)
{
    if (id >= ARRAY_SIZE(timer_tbl))  return (STE_ERR_BAD_ID);
    if (task_id == STE_TASK_ID_SELF) {
        task_id = task_current - task_tbl;
    } else if (task_id >= ARRAY_SIZE(task_tbl))  return (STE_ERR_BAD_ID);
    
    struct timercb *p = &timer_tbl[id];
    struct ste_timer *q = p->base;
    if (q->state != STE_TIMER_STATE_OFF)  return (STE_ERR_CLOBBER);
    
    ste_ticks_t now = ticks_get();
    
    q->state   = STE_TIMER_STATE_RUNNING;
    q->name    = name;
    q->type    = type;
    q->tmout   = tmout;
    q->start   = now;
    q->end     += now + tmout;
    q->task_id = task_id;
    q->signal  = signal;

    dllist_insert(p->list_node, dllist_end(timers_running));
    
    return (STE_OK);
}

/* Cancel a timer */

int task_timer_cancel(unsigned id)
{
    if (id >= ARRAY_SIZE(timer_tbl))  return (STE_ERR_BAD_ID);
    struct timercb *p = &timer_tbl[id];
    struct ste_timer *q = p->base;

    if (q->state == STE_TIMER_STATE_RUNNING) {
        q->state = STE_TIMER_STATE_OFF;

        dllist_erase(p->list_node);
    }

    return (STE_OK);
}

/* See if any timers have expired */

static void timers_run(void)
{
    ste_ticks_t now = ticks_get();

    struct dllist *r, *next;
    for (r = dllist_first(timers_running); r != dllist_end(timers_running); r = next) {
        next = r->next;
        struct timercb *p = FIELD_PTR_TO_STRUCT_PTR(r, struct timercb, list_node);
        struct ste_timer *q = p->base;

        if (q->state != STE_TIMER_STATE_RUNNING
            || ((long long) q->end - (long long) now) > 0
            ) {
            continue;
        }
        
        _task_signal(&task_tbl[q->task_id], q->signal);
        
        switch (q->type) {
        case STE_TIMER_TYPE_ONESHOT:
            q->state = STE_TIMER_STATE_OFF;
            dllist_erase(r);
            break;
        case STE_TIMER_TYPE_PERIODIC:
            q->start = q->end;
            q->end   = q->start + q->tmout;
            break;
        default:
            DEBUG_ASSERT(0);
        }
    }
}

/* Task executive initialization  */

void ste_init(void)
{
    rdyq_init();
    timers_init();
}

/* Task executive entry point */

void ste_run(void)
{
    task_resume();
}

/* Return task info */

int ste_task_info_get(unsigned id, struct ste_task *buf)
{
    struct tcb *p = task_id_to_ptr(id);
    if (p == 0)  return (STE_ERR_BAD_ID);

    *buf = *p->base;

    return (STE_OK);
}


static inline const char *state_str(unsigned state)
{
    static const char * const tbl[] = {
        "DEAD", "EXIT", "WAIT", "RDY", "RUN"
    };

    return (state < ARRAY_SIZE(tbl) ? tbl[state] : "UNK!");
}

static inline const char *wait_mode_str(unsigned wait_mode)
{
    static const char * const tbl[] = {
        "OR ", "AND"
    };

    return (wait_mode < ARRAY_SIZE(tbl) ? tbl[wait_mode] : "UNK!");
}

static inline const char *timer_state_str(unsigned state)
{
    static const char * const tbl[] = {
        "OFF", "RUN"
    };

    return (state < ARRAY_SIZE(tbl) ? tbl[state] : "UNK!");
}

static inline const char *timer_type_str(unsigned type)
{
    static const char * const tbl[] = {
        "ONESHOT", "PERIODIC"
    };

    return (type < ARRAY_SIZE(tbl) ? tbl[type] : "UNK!");
}

void ste_status_show(void)
{
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(task_tbl); ++i) {
        struct ste_task *p = task_tbl[i].base;
        printf("Task id %u name %s\n", i, p->name);
        printf("  state %s", state_str(p->state));
        if (p->state != STE_TASK_STATE_DEAD) {
            printf(" pri %d signals_got 0x%16llx signals_wait 0x%16llx\n",
                   p->pri, p->signals_got, p->signals_wait
                   );
            printf("  signals_wait_mode %s\n", wait_mode_str(p->signals_wait_mode));
            printf("  stack %p stack_top %p sp %p\n",
                   p->stack, p->stack_top, p->sp
                   );
            printf("  last_scheduled %llu total_time %llu", p->last_scheduled, p->total_time);
        }
        printf("\n");
    }

    for (i = 0; i < ARRAY_SIZE(timer_tbl); ++i) {
        struct ste_timer *p = timer_tbl[i].base;
        printf("Timer id %u name %s\n", i, p->name);
        printf("  state %s", timer_state_str(p->state));
        if (p->state != STE_TIMER_STATE_OFF) {
            printf(" type %s tmout %llu start %llu end %llu\n",
                   timer_type_str(p->type), p->tmout, p->start, p->end
                   );
            printf("  task id %u signals 0x%16llx",
                   p->task_id, p->signal
                   );
        }
        printf("\n");
    }
}
