#include "mycommon.h"

#ifndef TASK_QUEUE_H

typedef enum file_type_e
{
    FILE_TYPE_REGULAR = 0,
    FILE_TYPE_DIRECTORY = 1,
    FILE_TYPE_FIFO = 2,
} file_type_e;

typedef struct task_t
{
    int read_fd;
    int write_fd;
    char file_name[MAX_PATH_LEN];
    file_type_e file_type;
} task_t;

typedef struct task_queue_t
{
    task_t task;
    struct task_queue_t *next;
} task_queue_t;

/* task queue functions */
int offer_task (const task_t *task);

int poll_task (task_t *task);

int peek_task (task_t *task);

int task_queue_init ();

int task_queue_destroy ();

size_t task_queue_get_size ();

#endif