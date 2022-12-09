#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

#include "ste.h"

static void task_exit(void)
{
    struct ste_task buf[1];
    ste_task_info_get(STE_TASK_ID_SELF, buf);

    printf("Task %s exited\n", buf->name);
}

void (*ste_task_exit_hook)(void) = task_exit;

ste_ticks_t ticks_get(void)
{
    return (time(0));
}

#if 0

unsigned char task2_stack[4096];

void task2_main(void * arg)
{
    printf("%s starting, arg %s\n", __FUNCTION__, (char *) arg);

    task_timer_start(0, "", TIMER_TYPE_PERIODIC, 5, TASK_ID_SELF, 1);
    for (;;) {
        task_wait(1, 1, WAIT_MODE_OR);
        printf("%u: Task 2 timer expired\n", ticks_get());
        task_signal(3, 1);
    }
}

unsigned char task3_stack[4096];

void task3_main(void * arg)
{
    printf("%s starting, arg %s\n", __FUNCTION__, (char *) arg);

    task_timer_start(1, "", TIMER_TYPE_PERIODIC, 7, TASK_ID_SELF, 1);
    for (;;) {
        task_wait(1, 1, WAIT_MODE_OR);
        printf("%u: Task 3 timer expired\n", ticks_get());
        task_signal(3, 2);
    }
}

unsigned char task4_stack[4096];

void task4_main(void * arg)
{
    printf("%s starting, arg %s\n", __FUNCTION__, (char *) arg);

    for (;;) {
        task_wait(3, 3, WAIT_MODE_AND);
        printf("%u: Task 4\n", ticks_get());
        status_show();
    }
}

unsigned char task5_stack[4096];

void task5_main(void * arg)
{
    printf("%s starting, arg %s\n", __FUNCTION__, (char *) arg);

    sleep(3);

    printf("%s done\n", __FUNCTION__);
}

int main(void)
{
    tasks_init();

    printf("TASK_PRI_MIN = %d, TASK_PRI_MAX = %d\n", TASK_PRI_MIN, TASK_PRI_MAX);
    
    task_create(1, "Task 2", 10, task2_stack, sizeof(task2_stack), task2_main, "Arg for Task 2", 0);
    task_create(2, "Task 3", 1, task3_stack, sizeof(task3_stack), task3_main, "Arg for Task 3", 0);
    task_create(3, "Task 4", 1, task4_stack, sizeof(task4_stack), task4_main, "Arg for Task 4", 0);
    task_create(4, "Task 5", 19, task5_stack, sizeof(task5_stack), task5_main, "Arg for Task 5", 0);

    tasks_run();
}

#endif

#if 1                           /* Test round-robin scheduling of tasks with
                                   equal priority */

unsigned char server_stack[4096], client_stack[4096];
char server_mesg_buf[1024], client_mesg_buf[1024];

enum { MESG_SIGNAL = 1 << 0 };

void server_main(void * arg)
{
    printf("%s starting, arg %s\n", __FUNCTION__, (char *) arg);

    for (;;) {
        ste_task_wait(MESG_SIGNAL, MESG_SIGNAL, STE_TASK_WAIT_MODE_OR);

        printf("%s got %s\n", (char *) arg, server_mesg_buf);
        snprintf(client_mesg_buf, sizeof(client_mesg_buf),
                 "Reply %s", server_mesg_buf + 4
                 );
        ste_task_signal(1, MESG_SIGNAL);
    }
}

void client_main(void * arg)
{
    printf("%s starting, arg %s\n", __FUNCTION__, (char *) arg);

    enum { TIMER_SIGNAL = 1 << 1 };
    
    ste_timer_start(0, "Client timer", STE_TIMER_TYPE_PERIODIC, 1, STE_TASK_ID_SELF, TIMER_SIGNAL);
    
    unsigned cntr = 0;
    for (;;) {
        ste_task_wait(TIMER_SIGNAL, TIMER_SIGNAL, STE_TASK_WAIT_MODE_OR);
        
        ++cntr;
        snprintf(server_mesg_buf, sizeof(server_mesg_buf),
                 "Req %u", cntr
                 );

        ste_task_signal(0, MESG_SIGNAL);
        ste_task_wait(MESG_SIGNAL, MESG_SIGNAL, STE_TASK_WAIT_MODE_OR);

        printf("%s got %s\n", (char *) arg, client_mesg_buf);
    }
}

int main(void)
{
    ste_init();
    
    ste_task_create(0, "Server", 1, server_stack, sizeof(server_stack), server_main, "Server", 0);
    ste_task_create(1, "Client", 1, client_stack, sizeof(client_stack), client_main, "Client", 0);

    ste_run();
}

#endif
