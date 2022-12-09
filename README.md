# Simple Task Executive

## DESIGN
1. Execution is divided between ISRs and tasks.
   * ISRs execute asynchronously
   * Tasks execute synchronously
2. Everything requiring real-time response should happen in ISRs.
3. Tasks communicate with each other and ISRs communicate with tasks by sending signals and operating on shared data.
   * It is up to a task to mask interrupts if it is operating on data shared with an ISR.
   * Since task switching is synchornous, there is no need for synchronization primitives (e.g. mutexes, semaphores).
4. The **only** things an ISR can do are:
   * Operate on data shared with tasks
   * Send signals to tasks
   * ISRs **cannot** start or cancel tasks or timers.
     * Keeping these operations synchronous simplifies the design considerably.
5. Tasks are scheduled in strict priority order, or round-robin if of same priority.
6. Tasks can start and cancel timers.
7. Timers can be one-shot or periodic.
8. When a timer expires, it sends signal(s) to a task.
9. Number of tasks and number of timers are statically-defined.
10. Range of task priorities is small.
	* Linear search for ready tasks is reasonable.
11. Keep time with interrupts masked to a minimum.
