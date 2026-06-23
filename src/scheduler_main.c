#include "scheduler.h"
int main(void) {
    puts("=== TASK 1: PROCESS MANAGEMENT AND THREADING DEMO ===");
    run_threading_demo();
    run_race_condition_demo();
    run_round_robin_demo();
    run_deadlock_prevention_demo();
    return 0;
}

