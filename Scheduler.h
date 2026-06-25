#ifndef SCHEDULER_H
#define SCHEDULER_H

/*
 * Milestone 7 — Runtime scheduler selection infrastructure.
 *
 * Simulation code (sim.c) must call ONLY the three public dispatch functions
 * below. Algorithm selection is controlled at runtime via:
 *   ./sim -schd <fcfs|sjf> <input_file>
 *
 * Concrete FCFS/SJF logic lives in scheduler_fcfs.c and scheduler_sjf.c.
 */

typedef enum {
    SCHED_FCFS,
    SCHED_SJF
} SchedulerType;

typedef struct {
    SchedulerType type;
    const char *input_file;
} SchedulerConfig;

typedef struct SchedulerOps {
    void (*init_queue)(int intersection_id);
    void (*enqueue)(int intersection_id, int process_id, int burst_time);
    int  (*select_next)(int intersection_id);
    const char *display_name;
} SchedulerOps;

/* ---- Input validation (scheduler_args.c) ---- */
int parse_scheduler_args(int argc, char **argv, SchedulerConfig *out);
void print_scheduler_usage(const char *program_name);

/* ---- Central dispatcher (scheduler_core.c) ---- */
int scheduler_init(SchedulerType type);
void scheduler_shutdown(void);
SchedulerType scheduler_get_active_type(void);
const char *scheduler_get_active_name(void);

/*
 * Prepare one waiting queue per graph intersection (node).
 * Call after scheduler_init() and after the graph is loaded.
 */
void scheduler_configure(int num_intersections);

/*
 * ---- Public API used by sim.c ----
 *
 * intersection_id : node where travelers contend for entry
 * process_id      : traveler index / child identifier
 * burst_time      : remaining work (e.g. shared[tid].remainingNodes for SJF)
 *
 * select_next_process returns the process_id to wake, or -1 if no waiter exists.
 */
void init_scheduler_queue(int intersection_id);
void enqueue_waiting_process(int intersection_id, int process_id, int burst_time);
int select_next_process(int intersection_id);

#endif 
