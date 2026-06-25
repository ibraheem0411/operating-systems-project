#include "scheduler.h"

#include <stddef.h>

/*
 * FCFS scheduler — team implementation area.
 *
 * Replace the placeholder bodies below with your First-Come-First-Serve logic.
 * Typical approach:
 *   - Keep a per-intersection FIFO queue of waiting processes.
 *   - enqueue_waiting_process: append (process_id, burst_time) in arrival order.
 *   - select_next_process: dequeue the head of the queue for that intersection.
 *   - init_scheduler_queue: reset/clear the queue for one intersection.
 *
 * burst_time may be ignored for pure FCFS, but keep the parameter for a
 * uniform interface with SJF.
 */

static void fcfs_init_queue(int intersection_id)
{
    (void)intersection_id;

    /* TODO(FCFS): Initialize queue state for intersection_id. */
}

static void fcfs_enqueue(int intersection_id, int process_id, int burst_time)
{
    (void)intersection_id;
    (void)process_id;
    (void)burst_time;

    /* TODO(FCFS): Enqueue process_id at the tail of this intersection's queue. */
}

static int fcfs_select_next(int intersection_id)
{
    (void)intersection_id;

    /* TODO(FCFS): Dequeue and return the next process_id, or -1 if empty. */
    return -1;
}

const SchedulerOps fcfs_scheduler_ops = {
    .init_queue = fcfs_init_queue,
    .enqueue = fcfs_enqueue,
    .select_next = fcfs_select_next,
    .display_name = "FCFS"
};
