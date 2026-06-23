#ifndef SENTINELVAULT_MEMORY_SIMULATOR_H
#define SENTINELVAULT_MEMORY_SIMULATOR_H

#include "common.h"

typedef struct {
    int total_references;
    int hits;
    int faults;
    double hit_ratio;
    double miss_ratio;
} MemoryStats;

MemoryStats simulate_fifo(const int *refs, int ref_count, int frame_count, FILE *log_file);
MemoryStats simulate_lru(const int *refs, int ref_count, int frame_count, FILE *log_file);
void print_memory_stats(const char *algorithm, MemoryStats stats);
int write_memory_csv(const char *path, const int *refs, int ref_count, int frame_count);
void run_memory_demo(void);

#endif
