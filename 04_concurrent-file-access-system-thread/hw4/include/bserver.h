#ifndef B_SERVER_H
#define B_SERVER_H

#include "mycommon.h"
#include <signal.h>

extern sig_atomic_t signal_occured;

typedef struct cli_queue_t
{
    pid_t pid;
    struct cli_queue_t *next;
} cli_queue_t;

typedef struct cli_wait_queue_t
{
    pid_t pid;
    struct cli_wait_queue_t *next;
} cli_wait_queue_t;

int offer_client (pid_t pid);
int poll_client (pid_t *pid);
pid_t peek_client ();
void free_queue ();
void print_queue ();
int remove_client (pid_t pid);
size_t get_num_clients ();

int offer_client_w (pid_t pid);
int poll_client_w (pid_t *pid);
pid_t peek_client_w ();
void free_queue_w ();
void print_queue_w ();
size_t get_num_clients_w ();
int remove_client_w (pid_t pid);

void perform_command (const request_t *request, response_t *response);
#endif
