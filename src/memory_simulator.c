#include "memory_simulator.h"

static void print_frames(FILE *out, const int *frames, int count) {
    fputs("[", out);
    for (int i = 0; i < count; i++) {
        if (frames[i] < 0) fputs("-", out);
        else fprintf(out, "%d", frames[i]);
        if (i + 1 < count) fputs(", ", out);
    }
    fputs("]", out);
}

static MemoryStats finish_stats(int refs, int hits, int faults) {
    MemoryStats stats = {refs, hits, faults, 0.0, 0.0};
    if (refs > 0) {
        stats.hit_ratio = 100.0 * hits / refs;
        stats.miss_ratio = 100.0 * faults / refs;
    }
    return stats;
}

MemoryStats simulate_fifo(const int *refs, int ref_count, int frame_count, FILE *log_file) {
    int *frames = calloc((size_t)frame_count, sizeof(*frames));
    int hits = 0, faults = 0, next = 0;
    if (!refs || frame_count <= 0 || !frames) return finish_stats(0, 0, 0);
    for (int i = 0; i < frame_count; i++) frames[i] = -1;
    for (int step = 0; step < ref_count; step++) {
        int found = 0, evicted = -1;
        for (int i = 0; i < frame_count; i++) if (frames[i] == refs[step]) found = 1;
        if (found) hits++;
        else {
            faults++;
            evicted = frames[next];
            frames[next] = refs[step];
            next = (next + 1) % frame_count;
        }
        if (log_file) {
            fprintf(log_file, "Access %d -> %-5s", refs[step], found ? "HIT" : "FAULT");
            if (evicted >= 0) fprintf(log_file, " -> Evict %d", evicted);
            fputs(" -> Frames: ", log_file);
            print_frames(log_file, frames, frame_count);
            fputc('\n', log_file);
        }
    }
    free(frames);
    return finish_stats(ref_count, hits, faults);
}

MemoryStats simulate_lru(const int *refs, int ref_count, int frame_count, FILE *log_file) {
    int *frames = calloc((size_t)frame_count, sizeof(*frames));
    int *used = calloc((size_t)frame_count, sizeof(*used));
    int hits = 0, faults = 0;
    if (!refs || frame_count <= 0 || !frames || !used) {
        free(frames); free(used);
        return finish_stats(0, 0, 0);
    }
    for (int i = 0; i < frame_count; i++) frames[i] = -1;
    for (int step = 0; step < ref_count; step++) {
        int slot = -1, evicted = -1, found = 0;
        for (int i = 0; i < frame_count; i++) {
            if (frames[i] == refs[step]) {
                slot = i;
                found = 1;
            }
        }
        if (found) hits++;
        else {
            faults++;
            for (int i = 0; i < frame_count && slot < 0; i++) if (frames[i] < 0) slot = i;
            if (slot < 0) {
                slot = 0;
                for (int i = 1; i < frame_count; i++) if (used[i] < used[slot]) slot = i;
                evicted = frames[slot];
            }
            frames[slot] = refs[step];
        }
        used[slot] = step + 1;
        if (log_file) {
            fprintf(log_file, "Access %d -> %-5s", refs[step], found ? "HIT" : "FAULT");
            if (evicted >= 0) fprintf(log_file, " -> Evict %d", evicted);
            fputs(" -> Frames: ", log_file);
            print_frames(log_file, frames, frame_count);
            fputc('\n', log_file);
        }
    }
    free(frames); free(used);
    return finish_stats(ref_count, hits, faults);
}

void print_memory_stats(const char *algorithm, MemoryStats stats) {
    printf("%s Results: references=%d hits=%d faults=%d hit=%.2f%% miss=%.2f%%\n",
           algorithm, stats.total_references, stats.hits, stats.faults,
           stats.hit_ratio, stats.miss_ratio);
}

static int csv_algorithm(FILE *fp, const char *algorithm, const int *refs,
                         int ref_count, int frame_count, int use_lru) {
    int *frames = malloc((size_t)frame_count * sizeof(*frames));
    int *used = calloc((size_t)frame_count, sizeof(*used));
    int next = 0, hits = 0, faults = 0;
    if (!frames || !used) { free(frames); free(used); return SV_ERR; }
    for (int i = 0; i < frame_count; i++) frames[i] = -1;
    for (int step = 0; step < ref_count; step++) {
        int slot = -1, found = 0, evicted = -1;
        for (int i = 0; i < frame_count; i++) {
            if (frames[i] == refs[step]) { slot = i; found = 1; break; }
        }
        if (found) hits++;
        else {
            faults++;
            if (use_lru) {
                for (int i = 0; i < frame_count && slot < 0; i++)
                    if (frames[i] < 0) slot = i;
                if (slot < 0) {
                    slot = 0;
                    for (int i = 1; i < frame_count; i++)
                        if (used[i] < used[slot]) slot = i;
                    evicted = frames[slot];
                }
            } else {
                slot = next;
                evicted = frames[slot];
                next = (next + 1) % frame_count;
            }
            frames[slot] = refs[step];
        }
        used[slot] = step + 1;
        fprintf(fp, "%d,%s,%d,%s,", step + 1, algorithm, refs[step],
                found ? "HIT" : "FAULT");
        if (evicted >= 0) fprintf(fp, "%d", evicted);
        for (int i = 0; i < frame_count; i++) {
            fputc(',', fp);
            if (frames[i] >= 0) fprintf(fp, "%d", frames[i]);
        }
        fprintf(fp, ",%d,%d\n", hits, faults);
    }
    free(frames);
    free(used);
    return SV_OK;
}

int write_memory_csv(const char *path, const int *refs, int ref_count, int frame_count) {
    FILE *fp;
    if (!path || !refs || ref_count <= 0 || frame_count <= 0) return SV_ERR;
    fp = fopen(path, "w");
    if (!fp) return SV_ERR;
    fputs("step,algorithm,page,result,evicted", fp);
    for (int i = 0; i < frame_count; i++) fprintf(fp, ",frame%d", i + 1);
    fputs(",hits,faults\n", fp);
    int rc = csv_algorithm(fp, "FIFO", refs, ref_count, frame_count, 0);
    if (rc == SV_OK) rc = csv_algorithm(fp, "LRU", refs, ref_count, frame_count, 1);
    if (fclose(fp) != 0) rc = SV_ERR;
    return rc;
}

MemoryStats simulate_pointer_fifo(const int *refs, int ref_count, FILE *log_file) {
    int frames[4];
    int hits = 0;
    int faults = 0;
    int next = 0;

    if (!refs || ref_count <= 0) return finish_stats(0, 0, 0);
    for (int i = 0; i < 4; i++) frames[i] = -1;

    if (log_file) {
        fputs("--- Pointer based FIFO with four frames ---\n", log_file);
        fputs("Initial frames: ", log_file);
        print_frames(log_file, frames, 4);
        fputc('\n', log_file);
    }

    for (const int *cursor = refs; cursor < refs + ref_count; cursor++) {
        int page = *cursor;
        int found = 0;
        for (int i = 0; i < 4; i++) {
            if (*(frames + i) == page) found = 1;
        }

        if (found) {
            hits++;
        } else {
            int *replace_slot = &frames[next];
            int evicted = *replace_slot;
            *replace_slot = page;
            next = (next + 1) % 4;
            faults++;
            if (log_file) {
                fprintf(log_file, "Page %d loaded through pointer dereference", page);
                if (evicted >= 0) fprintf(log_file, " after evicting %d", evicted);
                fputs(" -> Frames: ", log_file);
                print_frames(log_file, frames, 4);
                fputc('\n', log_file);
            }
            continue;
        }

        if (log_file) {
            fprintf(log_file, "Page %d already exists -> Frames: ", page);
            print_frames(log_file, frames, 4);
            fputc('\n', log_file);
        }
    }

    return finish_stats(ref_count, hits, faults);
}

void run_memory_demo(void) {
    const int refs[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2};
    const int pointer_refs[] = {1, 2, 3, 4, 2, 5, 1, 6, 2, 7};
    const int count = (int)(sizeof(refs) / sizeof(refs[0]));
    const int pointer_count = (int)(sizeof(pointer_refs) / sizeof(pointer_refs[0]));
    puts("=== TASK 2: MEMORY MANAGEMENT SIMULATION ===");
    puts("Page size: 4096 bytes\nFrames: 3\n\n--- FIFO ---");
    MemoryStats fifo = simulate_fifo(refs, count, 3, stdout);
    print_memory_stats("FIFO", fifo);
    puts("\n--- LRU ---");
    MemoryStats lru = simulate_lru(refs, count, 3, stdout);
    print_memory_stats("LRU", lru);
    fputc('\n', stdout);
    MemoryStats pointer_fifo = simulate_pointer_fifo(pointer_refs, pointer_count, stdout);
    print_memory_stats("Pointer FIFO", pointer_fifo);
    if (write_memory_csv("task2_memory_trace.csv", refs, count, 3) == SV_OK)
        puts("\nCSV trace: task2_memory_trace.csv");
}
