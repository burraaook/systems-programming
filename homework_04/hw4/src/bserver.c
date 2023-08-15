#include "../include/bserver.h"

cli_queue_t *cli_queue_head = NULL;
cli_queue_t *cli_queue_tail = NULL;

cli_wait_queue_t *cli_wait_queue_head = NULL;
cli_wait_queue_t *cli_wait_queue_tail = NULL;

size_t num_clients = 0;
size_t num_clients_w = 0;

int offer_client(pid_t pid)
{
    cli_queue_t *new_node = NULL;

    if ((new_node = malloc(sizeof(cli_queue_t))) == NULL) {
        error_print("malloc");
        return -1;
    }

    new_node->pid = pid;
    new_node->next = NULL;

    if (cli_queue_head == NULL)
    {
        cli_queue_head = new_node;
        cli_queue_tail = new_node;
    }
    else
    {
        cli_queue_tail->next = new_node;
        cli_queue_tail = new_node;
    }

    num_clients++;
    return 0;
}

int poll_client(pid_t *pid)
{
    cli_queue_t *temp;

    if (cli_queue_head == NULL)
        return -1;

    *pid = cli_queue_head->pid;
    temp = cli_queue_head;
    cli_queue_head = cli_queue_head->next;
    free(temp);

    num_clients--;
    return 0;
}

void print_queue()
{
    cli_queue_t *temp;

    temp = cli_queue_head;

    printf("%ld - Queue: ", num_clients);
    while (temp != NULL)
    {
        printf("-%d-", temp->pid);
        temp = temp->next;
    }
    printf("\n");
}

void free_queue()
{
    cli_queue_t *temp;

    while (cli_queue_head != NULL)
    {
        temp = cli_queue_head;
        cli_queue_head = cli_queue_head->next;
        free(temp);
    }
}

pid_t peek_client()
{
    if (cli_queue_head == NULL)
        return -1;

    return cli_queue_head->pid;
}

int remove_client (pid_t pid)
{
    cli_queue_t *temp, *prev;

    if (cli_queue_head == NULL)
        return -1;

    if (cli_queue_head->pid == pid)
    {
        temp = cli_queue_head;
        cli_queue_head = cli_queue_head->next;

        if (cli_queue_head == NULL)
            cli_queue_tail = NULL;

        free(temp);
        num_clients--;
        return 0;
    }

    prev = cli_queue_head;
    temp = cli_queue_head->next;

    while (temp != NULL)
    {
        if (temp->pid == pid)
        {
            prev->next = temp->next;

            if (temp == cli_queue_tail)
                cli_queue_tail = prev;

            free(temp);
            num_clients--;
            return 0;
        }
        prev = temp;
        temp = temp->next;
    }

    return -1;
}

int offer_client_w (pid_t pid) 
{
    cli_wait_queue_t *new_node;

    if ((new_node = malloc(sizeof(cli_wait_queue_t))) == NULL) {
        error_print("malloc");
        return -1;
    }

    new_node->pid = pid;
    new_node->next = NULL;

    if (cli_wait_queue_head == NULL)
    {
        cli_wait_queue_head = new_node;
        cli_wait_queue_tail = new_node;
    }
    else
    {
        cli_wait_queue_tail->next = new_node;
        cli_wait_queue_tail = new_node;
    }

    num_clients_w++;
    return 0;
}
int poll_client_w (pid_t *pid)
{
    cli_wait_queue_t *temp;

    if (cli_wait_queue_head == NULL)
        return -1;

    *pid = cli_wait_queue_head->pid;
    temp = cli_wait_queue_head;
    cli_wait_queue_head = cli_wait_queue_head->next;
    free(temp);

    num_clients_w--;
    return 0;
}

int remove_client_w (pid_t pid)
{
    cli_wait_queue_t *temp, *prev;

    if (cli_wait_queue_head == NULL)
        return -1;

    if (cli_wait_queue_head->pid == pid)
    {
        temp = cli_wait_queue_head;
        cli_wait_queue_head = cli_wait_queue_head->next;

        if (cli_wait_queue_head == NULL)
            cli_wait_queue_tail = NULL;

        free(temp);
        num_clients_w--;
        return 0;
    }

    prev = cli_wait_queue_head;
    temp = cli_wait_queue_head->next;

    while (temp != NULL)
    {
        if (temp->pid == pid)
        {
            prev->next = temp->next;

            if (temp == cli_wait_queue_tail)
                cli_wait_queue_tail = prev;

            free(temp);
            num_clients_w--;
            return 0;
        }
        prev = temp;
        temp = temp->next;
    }

    return -1;
}

pid_t peek_client_w ()
{
    if (cli_wait_queue_head == NULL)
        return -1;

    return cli_wait_queue_head->pid;
}

void free_queue_w ()
{
    cli_wait_queue_t *temp;

    while (cli_wait_queue_head != NULL)
    {
        temp = cli_wait_queue_head;
        cli_wait_queue_head = cli_wait_queue_head->next;
        free(temp);
    }
}

void print_queue_w ()
{
    cli_wait_queue_t *temp;

    temp = cli_wait_queue_head;
    printf("%ld - ", num_clients_w);
    printf("Wait Queue: ");

    while (temp != NULL)
    {
        printf("%d ", temp->pid);
        temp = temp->next;
    }
    printf("\n");
}

size_t get_num_clients ()
{
    return num_clients;
}
size_t get_num_clients_w ()
{
    return num_clients_w;
}
