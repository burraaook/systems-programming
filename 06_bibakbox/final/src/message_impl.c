#include "../include/message_impl.h"

int send_message (int client_socket, const message_t *server_message, comp_result_t *comp_result)
{
    char buffer[MAX_BUF_LEN];
    /* socket is already open */
    memset(buffer, 0, MAX_BUF_LEN);

    /* convert server_message to buffer */
    if (message_to_buffer(server_message, comp_result, buffer) == -1)
        return -1;

    /* send buffer to client */
    if (write_str_msg(client_socket, buffer) == -1)
        return -1;

    return 0;
}

int receive_message (int client_socket, message_t *server_message, comp_result_t *comp_result)
{
    char buffer[MAX_BUF_LEN];
    /* socket is already open */
    memset(buffer, 0, MAX_BUF_LEN);

    /* receive buffer from client */
    if (read_str_msg(client_socket, buffer) == -1)
        return -1;

    /* convert buffer to server_message */
    if (buffer_to_message(buffer, server_message, comp_result) == -1)
        return -1;

    return 0;
}

int send_file (int client_socket, int file_fd)
{
    char buffer[MAX_BUF_LEN];
    ssize_t read_size;
    message_t server_message;
    memset(buffer, 0, MAX_BUF_LEN);
    ssize_t file_size;
    /* read file MAX_BUF_LEN bytes at a time and send to client */
    /* when it is done, send "DONE" message to client */

    /* check file size */
    lseek(file_fd, 0, SEEK_SET);
    file_size = lseek(file_fd, 0, SEEK_END);
    if (file_size == 0)
    {
        /* make sure file is really 0 bytes */
        sleep(1);
    }
    
    /* set fd to the beginning of the file */
    lseek(file_fd, 0, SEEK_SET);

    while ((read_size = read(file_fd, buffer, MAX_BUF_LEN)) > 0)
    {

        if (write(client_socket, buffer, read_size) == -1)
        {
            fprintf(stderr, "file send failed to socket %d\n", client_socket);
            error_print("write");
            return -1;
        }
        memset(buffer, 0, MAX_BUF_LEN);
        
        /* receive done message from receiver */
        memset(&server_message, 0, sizeof(message_t));
        if (receive_message(client_socket, &server_message, NULL) == -1)
        {
            fprintf(stderr, "message cannot be received\n");
            return -1;
        }

        if (server_message.command == CM_FAIL)
        {
            /* fail happened */
            break;
        }

        if (server_message.command != CM_DONE)
        {
            fprintf(stderr, "message is not DONE\n");
            return -1;
        }
    }

    /* send file end delimiter string to receiver */
    memset(buffer, 0, MAX_BUF_LEN);
    snprintf(buffer, MAX_BUF_LEN, "FILE_END_DELIMITER");
    if (write(client_socket, buffer, strlen(buffer)) == -1)
    {
        fprintf(stderr, "file send failed2\n");
        error_print("write");
        return -1;
    }

    /* receive "DONE" message from client */
    memset(&server_message, 0, sizeof(message_t));
    if (receive_message(client_socket, &server_message, NULL) == -1)
    {
        fprintf(stderr, "message cannot be received\n");
        return -1;
    }

    if (server_message.command != CM_DONE)
    {
        fprintf(stderr, "message is not DONE\n");
        return -1;
    }


    /* send "DONE" message to client */
    memset(&server_message, 0, sizeof(message_t));
    server_message.command = CM_DONE;

    if (send_message(client_socket, &server_message, NULL) == -1)
    {
        fprintf(stderr, "message cannot be sent\n");
        return -1;
    }

    return 0;
}

int receive_file_and_write (int client_socket, int fd)
{
    char buffer[MAX_BUF_LEN];
    ssize_t read_size;
    message_t server_message;

    /* receive file MAX_BUF_LEN bytes at a time and write to fd */
    /* when it is done, receive "DONE" message from client */

    /* set fd to the beginning of the file */
    lseek(fd, 0, SEEK_SET);
    while ((read_size = read(client_socket, buffer, MAX_BUF_LEN)) > 0)
    {
        if (strncmp(buffer, "FILE_END_DELIMITER", strlen("FILE_END_DELIMITER")) == 0)
            break;
        if (write(fd, buffer, read_size) == -1)
        {
            /* send fail message to sender */
            memset(&server_message, 0, sizeof(message_t));
            server_message.command = CM_FAIL;

            if (send_message(client_socket, &server_message, NULL) == -1)
            {
                fprintf(stderr, "message cannot be sent\n");
                return -1;
            }
            continue;
        }
        memset(buffer, 0, MAX_BUF_LEN);

        /* send done message to sender */
        memset(&server_message, 0, sizeof(message_t));
        server_message.command = CM_DONE;

        if (send_message(client_socket, &server_message, NULL) == -1)
        {
            fprintf(stderr, "message cannot be sent\n");
            return -1;
        }
    }

    /* send "DONE" message to receiver */
    memset(&server_message, 0, sizeof(message_t));
    server_message.command = CM_DONE;

    if (send_message(client_socket, &server_message, NULL) == -1)
    {
        fprintf(stderr, "message cannot be sent\n");
        return -1;
    }

    if (server_message.command != CM_DONE)
    {
        fprintf(stderr, "message is not DONE\n");
        return -1;
    }

    /* receive "DONE" message from sender */
    memset(&server_message, 0, sizeof(message_t));
    if (receive_message(client_socket, &server_message, NULL) == -1)
    {
        fprintf(stderr, "message cannot be received\n");
        return -1;
    }

    if (server_message.command != CM_DONE)
    {
        fprintf(stderr, "message is not DONE\n");
        return -1;
    }

    return 0;
}

int message_to_buffer (const message_t *server_message, const comp_result_t *comp_result, char *buffer)
{
    /* convert server_message to buffer */

    if (server_message->command == CM_EXIT)
    {
        snprintf(buffer, MAX_BUF_LEN, "COMMAND: CM_EXIT");
        return 0;
    }
    else if (server_message->command == CM_ACCEPT)
    {
        snprintf(buffer, MAX_BUF_LEN, "COMMAND: CM_ACCEPT");
        return 0;
    }
    else if (server_message->command == CM_REJECT)
    {
        snprintf(buffer, MAX_BUF_LEN, "COMMAND: CM_REJECT");
        return 0;
    }
    else if (server_message->command == CM_SAME)
    {
        snprintf(buffer, MAX_BUF_LEN, "COMMAND: CM_SAME");
        return 0;
    }
    else if (server_message->command == CM_CHANGE)
    {
        if (comp_result_to_buffer(comp_result, buffer) == -1)
            return -1;
        else
            return 0;
    }
    else if (server_message->command == CM_DONE)
    {
        snprintf(buffer, MAX_BUF_LEN, "COMMAND: CM_DONE");
        return 0;
    }
    else
    {
        snprintf(buffer, MAX_BUF_LEN, "COMMAND: CM_FAIL");
        return 0;
    }

    return 0;
}

int buffer_to_message (const char *buffer, message_t *server_message, comp_result_t *comp_result)
{
    /* convert buffer to server_message */

    if (strncmp(buffer, "COMMAND: CM_EXIT", strlen("COMMAND: CM_EXIT")) == 0)
    {
        server_message->command = CM_EXIT;
        return 0;
    }
    else if (strncmp(buffer, "COMMAND: CM_ACCEPT", strlen("COMMAND: CM_ACCEPT")) == 0)
    {
        server_message->command = CM_ACCEPT;
        return 0;
    }
    else if (strncmp(buffer, "COMMAND: CM_REJECT", strlen("COMMAND: CM_REJECT")) == 0)
    {
        server_message->command = CM_REJECT;
        return 0;
    }
    else if (strncmp(buffer, "COMMAND: CM_SAME", strlen("COMMAND: CM_SAME")) == 0)
    {
        server_message->command = CM_SAME;
        return 0;
    }
    else if (strncmp(buffer, "COMMAND: CM_DONE", strlen("COMMAND: CM_DONE")) == 0)
    {
        server_message->command = CM_DONE;
        return 0;
    }
    else if (strncmp(buffer, "COMMAND: CM_FAIL", strlen("COMMAND: CM_FAIL")) == 0)
    {
        server_message->command = CM_FAIL;
        return 0;
    }
    else
    {
        if (buffer_to_comp_result(buffer, comp_result) == -1)
            return -1;
        else
        {
            server_message->command = CM_CHANGE;
            return 0;
        }
    }

    return 0;
}
