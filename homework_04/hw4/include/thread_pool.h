
#include "mycommon.h"
#include "task_queue.h"
#include "command_impl.h"

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

/* thread pool */
typedef struct thread_pool_t
{
    pthread_t *threads;
    size_t num_threads;
    size_t num_threads_working;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_mutex_t log_mutex;
    pthread_rwlock_t rwlock;
    int shutdown;
    task_queue_t *task_queue;
} thread_pool_t;

/* thread pool functions */
int thread_pool_init (size_t num_threads);

int thread_pool_destroy ();

int thread_pool_submit (const request_t *task);

int thread_pool_get_size ();

int thread_pool_get_num_threads_working ();

void *worker_thread (void *args);

int execute_task (const request_t *task);

void log_msg (const char *log_file_name, const char *msg);

#endif