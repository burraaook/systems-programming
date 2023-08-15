#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "mycommon.h"
#include "task_queue.h"
#include "file_ops.h"
#include "message_impl.h"

typedef struct thread_pool_t
{
    pthread_t *threads;
    size_t active_threads;
    size_t num_threads;
    pthread_mutex_t mutex;
    pthread_rwlock_t rwlock;
    pthread_cond_t cond;
    char *dir_name;
    int shutdown;
} thread_pool_t;

/* thread pool functions */
int thread_pool_init (const size_t thread_pool_capacity, char *dir_name);

int thread_pool_destroy ();

int thread_pool_add_task (const task_t *task);

size_t thread_pool_get_active_threads ();

/* if the thread pool is full, return 1, else return 0 */
int thread_pool_is_full ();

void *worker_thread (void *args);

int client_worker (task_t *task);
            
int block_all_signals_thread ();

int unblock_all_signals_thread ();

int lock_mutex1 ();

int unlock_mutex1 ();
#endif