#include "memory_simulator.h"
#include <assert.h>
int main(void) {
    const int refs[] = {7,0,1,2,0,3,0,4,2,3,0,3,2};
    MemoryStats fifo = simulate_fifo(refs, 13, 3, NULL);
    MemoryStats lru = simulate_lru(refs, 13, 3, NULL);
    assert(fifo.faults == 10 && fifo.hits == 3);
    assert(lru.faults == 9 && lru.hits == 4);
    const char *trace_path = "test_memory_trace.csv";
    assert(write_memory_csv(trace_path, refs, 13, 3) == SV_OK);
    FILE *fp = fopen(trace_path, "r");
    assert(fp != NULL);
    char line[256];
    assert(fgets(line, sizeof(line), fp) != NULL);
    assert(strstr(line, "step,algorithm,page,result"));
    fclose(fp);
    unlink(trace_path);
    puts("test_memory: PASS");
    return 0;
}
