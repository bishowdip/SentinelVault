#ifndef SENTINELVAULT_PROCESS_DEMO_H
#define SENTINELVAULT_PROCESS_DEMO_H

#include "common.h"

typedef struct {
    pid_t parent_pid;
    pid_t child_pid;
    int child_exit_status;
    char message[128];
} ProcessDemoResult;

int run_process_ipc_demo(ProcessDemoResult *result, FILE *log_file);
void run_process_demo(void);

#endif
