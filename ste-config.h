typedef unsigned long long ste_ticks_t;

enum {
    /* Number of actual bits in a timer tick */
    STE_TICKS_BITS = 32,

    /* Maximum number of tasks */
    STE_MAX_TASKS  = 16,

    /* Maximum number of timers */
    STE_MAX_TIMERS = 16,

    /* Task priorities in range [-32, 31] */
    STE_TASK_PRI_BITS = 6,

    /* When the kernel needs to disable interrupts,
       it will disable to this level.
       It must be the highest level of any ISR calling
       the kernel.
     */
    STE_KERNEL_INTR_DISABLE_LVL = 16
};

extern void (*ste_task_exit_hook)(void);
