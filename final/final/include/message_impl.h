#ifndef MESSAGE_IMPL_H
#define MESSAGE_IMPL_H

#include "mycommon.h"
#include "file_ops.h"

typedef enum message_commands_t
{
    CM_EXIT = 0,
    CM_ACCEPT = 1,
    CM_REJECT = 2,
    CM_SAME = 3,
    CM_CHANGE = 4,
    CM_DONE = 5,
    CM_FAIL = 6
} message_commands_t;

typedef struct message_t
{
    message_commands_t command;
} message_t;

int send_message (int client_socket, const message_t*server_message, comp_result_t *comp_result);

int receive_message (int client_socket, message_t*server_message, comp_result_t *comp_result);

int send_file (int client_socket, int file_fd);

int receive_file_and_write (int client_socket, int fd);

int message_to_buffer (const message_t*server_message, const comp_result_t *comp_result, char *buffer);

int buffer_to_message (const char *buffer, message_t*server_message, comp_result_t *comp_result);

#endif