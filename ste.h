#include "ste-config.h"

typedef unsigned char bool;

unsigned cpu_intr_disable(unsigned lvl);
void cpu_intr_restore(unsigned old);
void cpu_intr_wait(void);

ste_ticks_t ticks_get(void);

#define STE_TASK_ID_SELF ((int) -1)

#define STE_TASK_PRI_MIN  ((int)((1 << (STE_TASK_PRI_BITS - 1)) - 1))
#define STE_TASK_PRI_MAX  ((int)(~STE_TASK_PRI_MIN))

enum {
    STE_OK            = 0,
    STE_ERR_BAD_ID    = -1,
    STE_ERR_BAD_PARAM = -2,
    STE_ERR_CLOBBER   = -3
};

typedef unsigned long long ste_task_signals_t;

struct ste_task {
    const char    *name;
    unsigned char state;
    signed char   pri;
    ste_task_signals_t signals_got, signals_wait;
    unsigned      signals_wait_mode;
    void          *stack, *stack_top, *sp;
    ste_ticks_t       last_scheduled, total_time;
    void          *data;
};

int ste_task_create(unsigned id, const char *name, int pri, void *stack,
                    unsigned stack_size, void *entry, void *arg, void *data
                    );

void ste_task_yield(void);

int ste_task_signal(unsigned id, ste_task_signals_t signals);
ste_task_signals_t ste_task_signals_get(void);
void ste_task_signals_clr(ste_task_signals_t signals);

enum {
    STE_TASK_WAIT_MODE_OR, STE_TASK_WAIT_MODE_AND
};

void ste_task_wait(ste_task_signals_t signals_clr, ste_task_signals_t signals_wait, unsigned wait_mode);

int ste_task_data_get(unsigned id , void **data);

enum {
    STE_TASK_STATE_DEAD = 0,
    STE_TASK_STATE_EXITED,
    STE_TASK_STATE_WAITING,
    STE_TASK_STATE_READY,
    STE_TASK_STATE_RUNNING
};

int ste_task_info_get(unsigned id, struct ste_task *buf);

enum {
    STE_TIMER_STATE_OFF = 0,
    STE_TIMER_STATE_RUNNING
};

enum {
    STE_TIMER_TYPE_ONESHOT,
    STE_TIMER_TYPE_PERIODIC
};

int ste_timer_start(unsigned id, const char *name, unsigned type, ste_ticks_t tmout,
                         unsigned task_id, ste_task_signals_t signal
                         );
int ste_timer_cancel(unsigned id);

struct ste_timer {
    const char    *name;
    unsigned char state, type;
    ste_ticks_t   tmout, start, end;
    unsigned      task_id;
    ste_task_signals_t signal;
};

void ste_timer_info_get(unsigned id, struct ste_timer *buf);

void ste_init(void);
void ste_run(void);
void ste_terminate(void);

void ste_status_show(void);
