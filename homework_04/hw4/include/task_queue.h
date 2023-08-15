#include "mycommon.h"

#ifndef TASK_QUEUE_H

typedef struct task_queue_t
{
    request_t task;
    struct task_queue_t *next;
} task_queue_t;

/* task queue functions */
int offer_task (const request_t *task);

int poll_task (request_t *task);

int peek_task (request_t *task);

int task_queue_init ();

int task_queue_destroy ();

size_t task_queue_get_size ();

#endif