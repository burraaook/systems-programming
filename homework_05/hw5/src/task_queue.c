#include "../include/task_queue.h"

task_queue_t *task_queue_head = NULL;
task_queue_t *task_queue_tail = NULL;

size_t task_queue_size = 0;

int offer_task (const task_t *task)
{
    task_queue_t *new_node;

    if ((new_node = malloc(sizeof(task_queue_t))) == NULL)
    {
        error_print("malloc");
        return -1;
    }

    new_node->task = *task;
    new_node->next = NULL;

    if (task_queue_head == NULL)
    {
        task_queue_head = new_node;
        task_queue_tail = new_node;
    }
    else
    {
        task_queue_tail->next = new_node;
        task_queue_tail = new_node;
    }

    task_queue_size++;
    return 0;
}

int poll_task (task_t *task)
{
    task_queue_t *temp;

    if (task_queue_head == NULL)
        return -1;

    *task = task_queue_head->task;
    temp = task_queue_head;
    task_queue_head = task_queue_head->next;
    free(temp);

    task_queue_size--;
    return 0;
}

int peek_task (task_t *task)
{
    if (task_queue_head == NULL)
        return -1;

    *task = task_queue_head->task;
    return 0;
}

int task_queue_init ()
{
    task_queue_head = NULL;
    task_queue_tail = NULL;
    task_queue_size = 0;
    return 0;
}

int task_queue_destroy ()
{
    task_queue_t *temp;

    while (task_queue_head != NULL)
    {
        temp = task_queue_head;
        task_queue_head = task_queue_head->next;
        free(temp);
    }

    task_queue_tail = NULL;
    task_queue_size = 0;
    return 0;
}

size_t task_queue_get_size ()
{
    return task_queue_size;
}