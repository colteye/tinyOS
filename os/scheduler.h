#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/**
 * @file scheduler.h
 * @brief Preemptive round-robin scheduler for ARM Cortex-M.
 *
 * Provides APIs for adding tasks, starting the scheduler, and handling
 * context switching.
 */

/**
 * @brief Create and register a new task with the scheduler.
 *
 * The user does not see or manipulate the task control block (TCB).
 *
 * @param func      Task entry function (no arguments, never returns).
 * @param stack     Pointer to caller-allocated stack memory (array of uint32_t).
 * @param size      Size of the stack array in words (uint32_t).
 * @param priority  Task priority (0 = highest, 31 = lowest).
 *
 * @note Automatically registers the task with the scheduler.
 */
void task_create(void (*func)(void),
                 uint32_t *stack,
                 uint32_t size,
                 uint8_t priority);

/**
 * @brief Put the current task to sleep for a given number of milliseconds.
 *
 * The task will be removed from the ready queue and not scheduled until the
 * specified time has passed.
 *
 * @param ms Sleep duration in milliseconds.
 */
void sleep(uint32_t ms);

/**
 * @brief Initialize the scheduler internals.
 *
 * Call this before adding any tasks.
 */
void scheduler_init(void);

/**
 * @brief Start the scheduler.
 *
 * - Initializes SysTick for 1 ms system ticks.
 * - Triggers the first PendSV context switch into the highest-priority task.
 * - Does not return.
 */
void scheduler_start(void);

/**
 * @brief Scheduler tick handler.
 *
 * Call from the timer interrupt only.
 *
 * Advances the system tick, updates sleep timers, and triggers task
 * preemption if needed.
 */
void scheduler_tick(void);

#endif // SCHEDULER_H