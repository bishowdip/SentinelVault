#ifndef SENTINELVAULT_JOB_QUEUE_H
#define SENTINELVAULT_JOB_QUEUE_H

#include "common.h"

typedef enum { JOB_CREATE_FILE, JOB_READ_FILE, JOB_WRITE_FILE, JOB_DELETE_FILE,
               JOB_ENCRYPT_FILE, JOB_DECRYPT_FILE, JOB_AUDIT_WRITE, JOB_STOP } JobType;

typedef struct {
    int job_id;
    JobType type;
    char username[MAX_USERNAME_LEN];
    char filename[MAX_FILENAME_LEN + 1];
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    int front, rear, count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} JobQueue;

int job_queue_init(JobQueue *queue);
int job_queue_push(JobQueue *queue, Job job);
int job_queue_pop(JobQueue *queue, Job *job);
void job_queue_destroy(JobQueue *queue);

#endif

