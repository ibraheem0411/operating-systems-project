#include "scheduler.h"

#include <stdio.h>
#include <string.h>

void print_scheduler_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s -schd <fcfs|sjf> <input_file>\n", program_name);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s -schd fcfs graph.txt\n", program_name);
    fprintf(stderr, "  %s -schd sjf  graph.txt\n", program_name);
}

static int parse_scheduler_name(const char *name, SchedulerType *out_type)
{
    if (name == NULL || out_type == NULL) {
        return -1;
    }

    if (strcmp(name, "fcfs") == 0) {
        *out_type = SCHED_FCFS;
        return 0;
    }

    if (strcmp(name, "sjf") == 0) {
        *out_type = SCHED_SJF;
        return 0;
    }

    return -1;
}

int parse_scheduler_args(int argc, char **argv, SchedulerConfig *out)
{
    if (out == NULL) {
        fprintf(stderr, "Error: scheduler configuration pointer is NULL.\n");
        return -1;
    }

    if (argv == NULL || argv[0] == NULL) {
        fprintf(stderr, "Error: invalid command-line arguments.\n");
        return -1;
    }

    if (argc != 4) {
        fprintf(stderr, "Error: expected 3 arguments after the program name.\n");
        print_scheduler_usage(argv[0]);
        return -1;
    }

    if (argv[1] == NULL || strcmp(argv[1], "-schd") != 0) {
        fprintf(stderr, "Error: missing or invalid flag '%s'. Expected '-schd'.\n",
                argv[1] != NULL ? argv[1] : "(null)");
        print_scheduler_usage(argv[0]);
        return -1;
    }

    if (argv[2] == NULL || argv[2][0] == '\0') {
        fprintf(stderr, "Error: scheduler name is missing.\n");
        print_scheduler_usage(argv[0]);
        return -1;
    }

    if (parse_scheduler_name(argv[2], &out->type) != 0) {
        fprintf(stderr,
                "Error: invalid scheduler '%s'. Supported values: fcfs, sjf.\n",
                argv[2]);
        print_scheduler_usage(argv[0]);
        return -1;
    }

    if (argv[3] == NULL || argv[3][0] == '\0') {
        fprintf(stderr, "Error: input file name is missing.\n");
        print_scheduler_usage(argv[0]);
        return -1;
    }

    out->input_file = argv[3];
    return 0;
}
