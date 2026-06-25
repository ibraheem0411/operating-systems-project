#include "scheduler.h"

#include <stdio.h>

extern const SchedulerOps fcfs_scheduler_ops;
extern const SchedulerOps sjf_scheduler_ops;

static const SchedulerOps *active_ops = NULL;
static SchedulerType active_type = SCHED_FCFS;

static const SchedulerOps *lookup_ops(SchedulerType type)
{
    switch (type) {
    case SCHED_FCFS:
        return &fcfs_scheduler_ops;
    case SCHED_SJF:
        return &sjf_scheduler_ops;
    default:
        return NULL;
    }
}

int scheduler_init(SchedulerType type)
{
    const SchedulerOps *ops = lookup_ops(type);
    if (ops == NULL) {
        fprintf(stderr, "Error: unsupported scheduler type.\n");
        return -1;
    }

    active_ops = ops;
    active_type = type;

    printf("[INIT] Active Scheduler: %s\n", active_ops->display_name);
    return 0;
}

void scheduler_shutdown(void)
{
    active_ops = NULL;
}

SchedulerType scheduler_get_active_type(void)
{
    return active_type;
}

const char *scheduler_get_active_name(void)
{
    if (active_ops == NULL || active_ops->display_name == NULL) {
        return "NONE";
    }

    return active_ops->display_name;
}

void scheduler_configure(int num_intersections)
{
    if (active_ops == NULL) {
        fprintf(stderr,
                "Error: scheduler_init() must be called before scheduler_configure().\n");
        return;
    }

    if (num_intersections <= 0) {
        fprintf(stderr, "Error: invalid intersection count (%d).\n", num_intersections);
        return;
    }

    for (int i = 0; i < num_intersections; i++) {
        init_scheduler_queue(i);
    }
}

void init_scheduler_queue(int intersection_id)
{
    if (active_ops != NULL && active_ops->init_queue != NULL) {
        active_ops->init_queue(intersection_id);
    }
}

void enqueue_waiting_process(int intersection_id, int process_id, int burst_time)
{
    if (active_ops != NULL && active_ops->enqueue != NULL) {
        active_ops->enqueue(intersection_id, process_id, burst_time);
    }
}

int select_next_process(int intersection_id)
{
    if (active_ops == NULL || active_ops->select_next == NULL) {
        return -1;
    }

    return active_ops->select_next(intersection_id);
}
