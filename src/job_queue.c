#include "job_queue.h"

int job_queue_init(JobQueue *q) {
    if (!q) return SV_ERR;
    memset(q, 0, sizeof(*q));
    if (pthread_mutex_init(&q->lock, NULL) != 0) return SV_ERR;
    if (pthread_cond_init(&q->not_empty, NULL) != 0) return SV_ERR;
    if (pthread_cond_init(&q->not_full, NULL) != 0) return SV_ERR;
    return SV_OK;
}

int job_queue_push(JobQueue *q, Job job) {
    pthread_mutex_lock(&q->lock);
    while (q->count == MAX_JOBS) pthread_cond_wait(&q->not_full, &q->lock);
    q->jobs[q->rear] = job;
    q->rear = (q->rear + 1) % MAX_JOBS;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return SV_OK;
}

int job_queue_pop(JobQueue *q, Job *job) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0) pthread_cond_wait(&q->not_empty, &q->lock);
    *job = q->jobs[q->front];
    q->front = (q->front + 1) % MAX_JOBS;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return SV_OK;
}

void job_queue_destroy(JobQueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

