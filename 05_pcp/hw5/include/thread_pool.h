#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "mycommon.h"
#include "task_queue.h"

typedef struct thread_pool_t
{
    pthread_t producer_thread;
    pthread_t *consumer_threads;
    size_t num_consumers;
    size_t num_consumers_working;
    pthread_mutex_t mutex;
    pthread_mutex_t stdout_mutex;
    pthread_cond_t empty;
    pthread_cond_t full;
    int producer_done;
    int terminate;
} thread_pool_t;

typedef struct producer_args_t
{
    char source_dir[MAX_FILENAME_LEN];
    char dest_dir[MAX_FILENAME_LEN];
    size_t buffer_size;
} producer_args_t;

typedef struct consumer_args_t
{
    char dest_dir[MAX_FILENAME_LEN];
} consumer_args_t;

int thread_pool_init (const size_t *num_consumers, producer_args_t *producer_args, consumer_args_t *consumer_args);
int thread_pool_start ();

int thread_pool_destroy ();

void *producer_worker (void *arg);

void *consumer_worker (void *arg);

int produce_files (const char *source_dir, const char *dest_dir, const size_t *buffer_size);

int process_task (const char *dest_dir, task_t *task);

int copy_file (const int source_fd, const int dest_fd, size_t *total_bytes);
#endif