#include "scheduler.h"

#include <stddef.h>

/*
 * SJF scheduler — team implementation area.
 *
 * Replace the placeholder bodies below with your Shortest-Job-First logic.
 * Typical approach:
 *   - Keep a per-intersection queue/list of waiting processes keyed by burst_time.
 *   - enqueue_waiting_process: insert process_id ordered by burst_time (tie-break FCFS).
 *   - select_next_process: pick the process with the smallest burst_time.
 *   - init_scheduler_queue: reset/clear the queue for one intersection.
 *
 * In this traffic simulation, burst_time usually maps to remaining path nodes
 * (see shared[i].remainingNodes in the child process).
 */

static void sjf_init_queue(int intersection_id)
{
    (void)intersection_id;

    /* TODO(SJF): Initialize queue state for intersection_id. */
}

static void sjf_enqueue(int intersection_id, int process_id, int burst_time)
{
    (void)intersection_id;
    (void)process_id;
    (void)burst_time;

    /* TODO(SJF): Insert process_id ordered by burst_time for this intersection. */
}

static int sjf_select_next(int intersection_id)
{
    (void)intersection_id;

    /* TODO(SJF): Return the shortest-job process_id, or -1 if empty. */
    return -1;
}

const SchedulerOps sjf_scheduler_ops = {
    .init_queue = sjf_init_queue,
    .enqueue = sjf_enqueue,
    .select_next = sjf_select_next,
    .display_name = "SJF"
};
