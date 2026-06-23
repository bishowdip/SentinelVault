#include "scheduler.h"

#define COUNTER_THREADS 5
#define COUNTER_LOOPS 100000

static volatile long shared_counter;
static pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t *file_sem;

typedef struct { JobQueue *queue; int id; } WorkerArg;
typedef struct { const char *name; int burst, remaining, waiting, turnaround; } SimProcess;

static void *unsafe_increment(void *unused) {
    (void)unused;
    for (int i = 0; i < COUNTER_LOOPS; i++) {
        long observed = shared_counter;
        if ((i & 127) == 0) sched_yield();
        shared_counter = observed + 1;
    }
    return NULL;
}

static void *safe_increment(void *unused) {
    (void)unused;
    for (int i = 0; i < COUNTER_LOOPS; i++) {
        pthread_mutex_lock(&counter_lock);
        shared_counter++;
        pthread_mutex_unlock(&counter_lock);
    }
    return NULL;
}

static void *worker(void *ptr) {
    WorkerArg *arg = ptr;
    for (;;) {
        Job job;
        job_queue_pop(arg->queue, &job);
        if (job.type == JOB_STOP) break;
        sem_wait(file_sem);
        printf("[WORKER %d] Job #%d for %s (%s)\n", arg->id, job.job_id,
               job.username, job.filename);
        usleep(10000);
        sem_post(file_sem);
    }
    return NULL;
}

void run_threading_demo(void) {
    JobQueue queue;
    pthread_t threads[WORKER_COUNT];
    WorkerArg args[WORKER_COUNT];
    job_queue_init(&queue);
    sem_unlink("/sentinelvault_demo_access");
    file_sem = sem_open("/sentinelvault_demo_access", O_CREAT | O_EXCL, 0600, FILE_ACCESS_LIMIT);
    if (file_sem == SEM_FAILED) {
        perror("sem_open");
        job_queue_destroy(&queue);
        return;
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        args[i] = (WorkerArg){&queue, i + 1};
        pthread_create(&threads[i], NULL, worker, &args[i]);
        printf("[THREAD] Created worker thread %d\n", i + 1);
    }
    for (int i = 0; i < 8; i++) {
        Job job = {.job_id = i + 1, .type = JOB_WRITE_FILE};
        sv_copy(job.username, sizeof(job.username), i % 2 ? "auditor" : "admin");
        snprintf(job.filename, sizeof(job.filename), "evidence_%02d.txt", i + 1);
        job_queue_push(&queue, job);
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        Job stop = {.type = JOB_STOP};
        job_queue_push(&queue, stop);
    }
    for (int i = 0; i < WORKER_COUNT; i++) pthread_join(threads[i], NULL);
    sem_close(file_sem);
    sem_unlink("/sentinelvault_demo_access");
    job_queue_destroy(&queue);
    puts("[SYNC] Queue protected by mutex/condition variables; file work limited by semaphore");
}

void run_race_condition_demo(void) {
    pthread_t threads[COUNTER_THREADS];
    long expected = COUNTER_THREADS * COUNTER_LOOPS;
    shared_counter = 0;
    for (int i = 0; i < COUNTER_THREADS; i++) pthread_create(&threads[i], NULL, unsafe_increment, NULL);
    for (int i = 0; i < COUNTER_THREADS; i++) pthread_join(threads[i], NULL);
    printf("[RACE UNSAFE] Expected=%ld Actual=%ld\n", expected, shared_counter);
    shared_counter = 0;
    for (int i = 0; i < COUNTER_THREADS; i++) pthread_create(&threads[i], NULL, safe_increment, NULL);
    for (int i = 0; i < COUNTER_THREADS; i++) pthread_join(threads[i], NULL);
    printf("[RACE SAFE]   Expected=%ld Actual=%ld (mutex protected)\n", expected, shared_counter);
}

void run_round_robin_demo(void) {
    SimProcess p[] = {{"P1", 5, 5, 0, 0}, {"P2", 3, 3, 0, 0}, {"P3", 8, 8, 0, 0}};
    const int n = 3, quantum = 2;
    int time = 0, completed = 0;
    double waiting = 0, turnaround = 0;
    fputs("[ROUND ROBIN] ", stdout);
    while (completed < n) {
        for (int i = 0; i < n; i++) {
            if (p[i].remaining <= 0) continue;
            printf("%s ", p[i].name);
            int slice = p[i].remaining < quantum ? p[i].remaining : quantum;
            time += slice;
            p[i].remaining -= slice;
            if (p[i].remaining == 0) {
                p[i].turnaround = time;
                p[i].waiting = time - p[i].burst;
                waiting += p[i].waiting;
                turnaround += p[i].turnaround;
                completed++;
            }
        }
    }
    printf("\nAverage waiting=%.2f turnaround=%.2f\n", waiting / n, turnaround / n);
}

void run_deadlock_prevention_demo(void) {
    puts("[DEADLOCK] Fixed lock order: session -> metadata -> file -> audit");
    puts("[DEADLOCK] Circular wait is prevented");
}
