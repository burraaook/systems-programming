#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "mycommon.h"

typedef struct task_t
{
    int client_socket;
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