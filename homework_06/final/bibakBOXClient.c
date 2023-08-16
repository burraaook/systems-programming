#include "include/thread_pool.h"
#include "include/mycommon.h"
#include "include/message_impl.h"
#include "include/file_ops.h"

sig_atomic_t signal_occurred = 0;

/* log file name */
char *log_file_name_client = "bibakBOXClient_LOG.BIBAKLOG";

int log_function (char *log_message, const char *client_path, const comp_result_t *comp_result);

void signal_handler (int signal)
{
    if (signal == SIGINT)
        signal_occurred = 1;
}

int bibak_client (int client_socket, const char *dir_name);

int main (int argc, char *argv[])
{
    char *dir_name;
    int port_number;
    int client_socket;
    struct sigaction sa;
    char buffer[MAX_BUF_LEN];
    struct sockaddr_in server_address;
    size_t len;

    /* input format is ./bibakBOXClient <dirName> <port number> */
    /* other possible format is: ./bibakBOXClient <dirName> <port number> <server IP> */

    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "Usage1: %s <dirName> <port number>\n", argv[0]);
        fprintf(stderr, "Usage2: %s <dirName> <port number> <server IP>\n", argv[0]);
        return -1;
    }

    setbuf(stdout, NULL);

    /* register signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    block_all_signals_thread();

    /* assign directory name */
    dir_name = argv[1];

    /* assign port number */
    port_number = string_to_int(argv[2], strlen(argv[2]));

    /* print inputs */
    printf("dirName: %s\n", dir_name);
    printf("port number: %d\n", port_number);

    /* check if directory exists, if not create it */
    if (access(argv[1], F_OK) == -1)
    {
        if (mkdir(argv[1], 0777) == -1)
        {
            error_print("mkdir");
            unblock_all_signals_thread();
            return -1;
        }
    }

    /* create socket */
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_print("socket");
        unblock_all_signals_thread();
        return -1;
    }

    /* connect socket */
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_number);

    if (argc == 4)
    {
        if (inet_pton(AF_INET, argv[3], &server_address.sin_addr) == -1)
        {
            error_print("inet_pton");
            unblock_all_signals_thread();
            return -1;
        }
    }
    else
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    
    unblock_all_signals_thread();

    if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1)
    {
        if (errno == EINTR)
            fprintf(stderr, "SIGINT occurred, exiting...\n");
        else
            error_print("connect");
        return -1;
    }

    /* log the connection */
    snprintf(buffer, MAX_BUF_LEN, "CLIENT: Connected to the port: %d\n", port_number);
    if (log_function(buffer, dir_name, NULL) == -1)
    {
        fprintf(stderr, "CLIENT: Cannot log the connection\n");
    }

    while (1)
    {
        /* check if signal occurred */
        if (signal_occurred)
        {
            fprintf(stderr, "Signal occurred, exiting...\n");
            
            break;
        }

        len = 0;
        /* receive message from server */
        memset(buffer, 0, MAX_BUF_LEN);
        if (read_str_msg(client_socket, buffer) == -1)
        {
            if (errno != EINTR)
                error_print("read");

            continue;
        }

        len = strlen("Connection SUCCESSFUL");
        if (strlen(buffer) == len && strncmp(buffer, "Connection SUCCESSFUL", len) == 0)
            fprintf(stdout, "SERVER: %s\n", buffer);
        else
        {
            fprintf(stderr, "SERVER: %s\nTerminating...\n", buffer);
            break;
        }

        /* send message to server */
        memset(buffer, 0, MAX_BUF_LEN);
        snprintf(buffer, MAX_BUF_LEN, "Connection SUCCESSFUL");
        
        if (write_str_msg(client_socket, buffer) == -1)
        {
            if (errno != EINTR)
                error_print("write");

            continue;
        }

        /* communicate with server repeatedly */
        if (bibak_client(client_socket, dir_name) == -1)
        {
            fprintf(stderr, "CLIENT: An error occured terminating...\n");
            break;
        }
        else
            break;
    }

    close(client_socket);
    return 0;
}

int bibak_client (int client_socket, const char *dir_name)
{
    char buffer[MAX_BUF_LEN];
    char log_buffer[MAX_BUF_LEN];
    comp_result_t comp_result_server;
    message_t message;
    int file_fd;
    comp_result_list_t *comp_list = NULL;
    file_info_list_t *head_prev = NULL;
    file_info_list_t *tail_prev = NULL;
    file_info_list_t *head_cur = NULL;
    file_info_list_t *tail_cur = NULL;
    comp_result_list_t *comp_list_temp = NULL;
    int change_flag = 0;
    int accept_flag = 0;
    int delete_flag = 0;

    while (1)
    {
        /* receive message from server */
        memset(&message, 0, sizeof(message_t));
        memset(&comp_result_server, 0, sizeof(comp_result_t));

        /* server to client */

        /* receive files till CM_DONE, CM_EXIT or CM_SAME */
        while (1)
        {
            memset(&message, 0, sizeof(message_t));
            memset(&comp_result_server, 0, sizeof(comp_result_t));

            block_all_signals_thread();
            if (receive_message(client_socket, &message, &comp_result_server) == -1)
            {
                fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
            else if (message.command == CM_DONE)
            {
                unblock_all_signals_thread();
                break;
            }
            else if (message.command == CM_EXIT)
            {
                fprintf(stderr, "CLIENT: Termination message received from server\n");
                log_function("CLIENT: Termination message received from server\n", dir_name, NULL);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
            else if (message.command == CM_SAME)
            {
                unblock_all_signals_thread();
                break;
            }
            else if (message.command == CM_CHANGE)
            {
                unblock_all_signals_thread();
                
                /* check file stats */
                accept_flag = 0;
                if (is_current_old(&comp_result_server, dir_name) == 1)
                    accept_flag = 1;
                
                if (accept_flag == 1 && comp_result_server.file_info.file_type == DIRECTORY_TYPE)
                {
                    /* deletion */
                    if (comp_result_server.comp_result == FILE_DELETED)
                    {
                        block_all_signals_thread();
                        /* delete all files inside it */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result_server.file_info.path);
                        if (delete_all_files_and_dir(buffer, &head_cur, &tail_cur, dir_name) == -1)
                        {
                            accept_flag = 0;
                        }
                        else
                        {
                            /* log the deletion */
                            memset(log_buffer, 0, MAX_BUF_LEN);
                            snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Deleting directory %s\n", comp_result_server.file_info.path);
                            if (log_function(log_buffer, dir_name, &comp_result_server) == -1)
                            {
                                fprintf(stderr, "CLIENT: Cannot log the deletion\n");
                            }

                            /* send done message */
                            memset(&message, 0, sizeof(message_t));
                            message.command = CM_DONE;

                            if (send_message(client_socket, &message, NULL) == -1)
                            {
                                fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                                log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                                destroy_file_info_list_prev(&head_prev, &tail_prev);
                                destroy_file_info_list_current(&head_cur, &tail_cur);
                                free_comp_result_list(&comp_list);
                                return 0;
                            }
                            unblock_all_signals_thread();
                            continue;
                        }
                        unblock_all_signals_thread();
                    }
                    else if (comp_result_server.comp_result == FILE_ADDED || comp_result_server.comp_result == FILE_MODIFIED)
                    {
                        block_all_signals_thread();

                        /* create directories before it */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result_server.file_info.path);
                        if (create_dirs(buffer, &head_cur, &tail_cur, dir_name) == -1)
                        {
                            fprintf(stderr, "CLIENT: Cannot create directories before %s\n", comp_result_server.file_info.path);
                            accept_flag = 0;
                        }
                        else
                        {
                            /* create directory */
                            if (mkdir(buffer, 0777) == -1)
                            {
                                /* directory already exists */
                                accept_flag = 0;
                            }
                            else
                            {

                                /* log the creation */
                                memset(log_buffer, 0, MAX_BUF_LEN);
                                snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Creating directory %s\n", comp_result_server.file_info.path);
                                if (log_function(log_buffer, dir_name, &comp_result_server) == -1)
                                {
                                    fprintf(stderr, "CLIENT: Cannot log the creation\n");
                                }

                                /* update given file info in current list */
                                if (update_file_info_list(&comp_result_server, &head_cur, &tail_cur, dir_name) == -1)
                                {
                                    fprintf(stderr, "CLIENT: Cannot update file info list\n");
                                }

                                /* send done message and continue */
                                memset(&message, 0, sizeof(message_t));
                                message.command = CM_DONE;

                                if (send_message(client_socket, &message, NULL) == -1)
                                {
                                    fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                                    log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                                    destroy_file_info_list_prev(&head_prev, &tail_prev);
                                    destroy_file_info_list_current(&head_cur, &tail_cur);
                                    free_comp_result_list(&comp_list);
                                    return 0;
                                }

                                unblock_all_signals_thread();
                                continue;
                            }
                        }
                        unblock_all_signals_thread();
                    }
                }
                else if (accept_flag == 1 && comp_result_server.file_info.file_type == FIFO_FILE_TYPE)
                {
                    /* deletion */
                    if (comp_result_server.comp_result == FILE_DELETED)
                    {
                        block_all_signals_thread();
                        /* delete fifo file */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result_server.file_info.path);
                        if (unlink(buffer) == -1)
                        {
                            accept_flag = 0;
                        }
                        else
                        {
                            /* update given file info in current list */
                            if (update_file_info_list(&comp_result_server, &head_cur, &tail_cur, dir_name) == -1)
                            {
                                fprintf(stderr, "CLIENT: Cannot update file info list\n");
                            }

                            /* log the deletion */
                            memset(log_buffer, 0, MAX_BUF_LEN);
                            snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Deleting fifo file %s\n", comp_result_server.file_info.path);
                            if (log_function(log_buffer, dir_name, &comp_result_server) == -1)
                            {
                                fprintf(stderr, "CLIENT: Cannot log the deletion\n");
                            }

                            /* send done message */
                            memset(&message, 0, sizeof(message_t));
                            message.command = CM_DONE;

                            if (send_message(client_socket, &message, NULL) == -1)
                            {
                                fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                                log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                                destroy_file_info_list_prev(&head_prev, &tail_prev);
                                destroy_file_info_list_current(&head_cur, &tail_cur);
                                free_comp_result_list(&comp_list);
                                return 0;
                            }

                            unblock_all_signals_thread();
                            continue;
                        }
                        unblock_all_signals_thread();
                    }
                    else if (comp_result_server.comp_result == FILE_ADDED || comp_result_server.comp_result == FILE_MODIFIED)
                    {
                        block_all_signals_thread();
                        /* create directories before it */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result_server.file_info.path);
                        if (create_dirs(buffer, &head_cur, &tail_cur, dir_name) == -1)
                        {
                            fprintf(stderr, "CLIENT: Cannot create directories before %s\n", comp_result_server.file_info.path);
                            accept_flag = 0;
                        }
                        else
                        {
                            /* create fifo file */
                            if (mkfifo(buffer, 0777) == -1)
                            {
                                fprintf(stderr, "CLIENT: Cannot create fifo file %s\n", comp_result_server.file_info.path);
                                accept_flag = 0;
                            }
                            else
                            {

                                /* log the update */
                                memset(log_buffer, 0, MAX_BUF_LEN);
                                snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Updating fifo file %s\n", comp_result_server.file_info.path);
                                if (log_function(log_buffer, dir_name, &comp_result_server) == -1)
                                {
                                    fprintf(stderr, "CLIENT: Cannot log the creation\n");
                                }

                                /* update given file info in current list */
                                if (update_file_info_list(&comp_result_server, &head_cur, &tail_cur, dir_name) == -1)
                                {
                                    fprintf(stderr, "CLIENT: Cannot update file info list\n");
                                }

                                /* send done message and continue */
                                memset(&message, 0, sizeof(message_t));
                                message.command = CM_DONE;

                                if (send_message(client_socket, &message, NULL) == -1)
                                {
                                    fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                                    log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                                    destroy_file_info_list_prev(&head_prev, &tail_prev);
                                    destroy_file_info_list_current(&head_cur, &tail_cur);
                                    free_comp_result_list(&comp_list);
                                    return 0;
                                }

                                unblock_all_signals_thread();
                                continue;
                            }
                        }
                        unblock_all_signals_thread();
                    }
                }

                if (accept_flag == 1)
                {
                    delete_flag = 0;
                    if (comp_result_server.comp_result == FILE_DELETED)
                        delete_flag = 1;

                    /* open file and create directories if necessary, and if it is not deleted */                    
                    block_all_signals_thread();
                    if (delete_flag == 0 && 
                        (open_file_and_create_dirs(&comp_result_server, dir_name, &file_fd, &head_cur, &tail_cur) == -1))
                    {
                        /* send reject message to server */
                        memset(&message, 0, sizeof(message_t));
                        message.command = CM_REJECT;

                        if (send_message(client_socket, &message, &comp_result_server) == -1)
                        {
                            fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                            log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                            destroy_file_info_list_prev(&head_prev, &tail_prev);
                            destroy_file_info_list_current(&head_cur, &tail_cur);
                            free_comp_result_list(&comp_list);
                            return 0;
                        }

                        unblock_all_signals_thread();
                        continue;
                    }                   

                    /* send accept message to server */
                    memset(&message, 0, sizeof(message_t));
                    message.command = CM_ACCEPT;

                    if (send_message(client_socket, &message, &comp_result_server) == -1)
                    {                        
                        if (delete_flag == 0)
                            close(file_fd);
                        return -1;
                    }

                    /* log file content receiving */
                    memset(log_buffer, 0, MAX_BUF_LEN);
                    snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Receiving file %s from server\n", comp_result_server.file_info.path);
                    if (log_function(log_buffer, dir_name, &comp_result_server) == -1)
                    {
                        fprintf(stderr, "CLIENT: Cannot log the file content receiving\n");
                    }

                    /* receive file from server */
                    if  (delete_flag == 0 && 
                        (receive_file_and_write(client_socket, file_fd) == -1))
                    {
                        close(file_fd);
                        fprintf(stderr, "CLIENT: Cannot receive file from server, terminating...\n");
                        return -1;
                    }
                    else if (delete_flag == 1)
                    {
                        /* delete file */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result_server.file_info.path);
                        unlink(buffer);

                        /* log the deletion */
                        memset(log_buffer, 0, MAX_BUF_LEN);
                        snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Deleting file %s\n", comp_result_server.file_info.path);
                        if (log_function(log_buffer, dir_name, &comp_result_server) == -1)
                        {
                            fprintf(stderr, "CLIENT: Cannot log the deletion\n");
                        }
                    }

                    /* close file */
                    if (delete_flag == 0)
                        close(file_fd);

                    /* send done message to server */
                    memset(&message, 0, sizeof(message_t));
                    message.command = CM_DONE;

                    if (send_message(client_socket, &message, NULL) == -1)
                    {
                        fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                        log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }
                    unblock_all_signals_thread();

                    /* update given file info in current list */
                    if (update_file_info_list(&comp_result_server, &head_cur, &tail_cur, dir_name) == -1)
                    {
                        fprintf(stderr, "CLIENT: Cannot update file info list\n");
                        continue;
                    }
                }
                else
                {
                    /* send reject message to server */
                    memset(&message, 0, sizeof(message_t));
                    message.command = CM_REJECT;

                    block_all_signals_thread();
                    if (send_message(client_socket, &message, &comp_result_server) == -1)
                    {
                        fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                        log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }
                    unblock_all_signals_thread();
                }
            }
            else 
            {
                fprintf(stderr, "CLIENT: Unknown message received from server\n");
                log_function("CLIENT: Unknown message received from server\n", dir_name, NULL);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return -1;
            }
            unblock_all_signals_thread();
        }

        /* check termination signal */
        if (signal_occurred)
        {
            fprintf(stderr, "Signal occurred, exiting...\n");
            
            /* send exit message to server */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_EXIT;

            /* log the termination */
            snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: SIGINT Received, sending exit message to server, and terminating...\n");
            if (log_function(log_buffer, dir_name, NULL) == -1)
            {
                fprintf(stderr, "CLIENT: Cannot log the termination\n");
            }

            block_all_signals_thread();
            if (send_message(client_socket, &message, &comp_result_server) == -1)
            {
                fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
            unblock_all_signals_thread();

            destroy_file_info_list_prev(&head_prev, &tail_prev);
            destroy_file_info_list_current(&head_cur, &tail_cur);
            free_comp_result_list(&comp_list);
            return 0;
        }

        /* client to server */
        if (change_flag == 1)
        {
            /* send message for each file */
            comp_list_temp = comp_list;

            while (comp_list_temp != NULL)
            {
                memset(&message, 0, sizeof(message_t));
                message.command = CM_CHANGE;

                delete_flag = 0;
                if (comp_list_temp->comp_result.comp_result == FILE_DELETED)
                    delete_flag = 1;

                if (delete_flag == 0)
                {
                    /* add dirname to path */
                    snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_list_temp->comp_result.file_info.path);
                 
                    if (comp_list_temp->comp_result.file_info.file_type == REGULAR_FILE_TYPE)
                    {
                        /* open file */
                        if ((file_fd = open(buffer, O_RDONLY)) == -1)
                        {
                            error_print("open");
                            comp_list_temp = comp_list_temp->next;
                            continue;
                        }     
                    }
                }

                if (send_message(client_socket, &message, &comp_list_temp->comp_result) == -1)
                {
                    fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                    log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                    destroy_file_info_list_prev(&head_prev, &tail_prev);
                    destroy_file_info_list_current(&head_cur, &tail_cur);
                    free_comp_result_list(&comp_list);
                    return 0;
                }

                /* receive feedback from server */
                memset(&message, 0, sizeof(message_t));
                if (receive_message(client_socket, &message, &comp_list_temp->comp_result) == -1)
                {
                    fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                    log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                    destroy_file_info_list_prev(&head_prev, &tail_prev);
                    destroy_file_info_list_current(&head_cur, &tail_cur);
                    free_comp_result_list(&comp_list);
                    return 0;
                }

                if (message.command == CM_DONE)
                {
                    /* it means given file is directory or fifo, so no need to send file */
                    comp_list_temp = comp_list_temp->next;
                    continue;
                }

                if (message.command == CM_REJECT)
                {
                    if (delete_flag == 0)
                        close(file_fd);
                    comp_list_temp = comp_list_temp->next;
                    continue;
                }

                /* if it is accepted */
                if (message.command == CM_ACCEPT && delete_flag == 0)
                {
                    /* send file to server */
                    if (send_file(client_socket, file_fd) == -1)
                    {
                        fprintf(stderr, "cannot send file to server, terminating...\n");
                        close(file_fd);
                        return -1;
                    }

                    close(file_fd);

                    /* log file content sending */
                    memset(log_buffer, 0, MAX_BUF_LEN);
                    snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Sending file %s to server\n", comp_list_temp->comp_result.file_info.path);
                    if (log_function(log_buffer, dir_name, &comp_list_temp->comp_result) == -1)
                    {
                        fprintf(stderr, "CLIENT: Cannot log the file content sending\n");
                    }

                    /* receive done message from server */
                    memset(&message, 0, sizeof(message_t));
                    if (receive_message(client_socket, &message, &(comp_list_temp->comp_result)) == -1)
                    {
                        fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                        log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }

                    if (message.command != CM_DONE)
                    {
                        comp_list_temp = comp_list_temp->next;
                        continue;
                    }
                }

                if (message.command == CM_ACCEPT && delete_flag == 1)
                {
                    /* receive done message from server */
                    memset(&message, 0, sizeof(message_t));
                    if (receive_message(client_socket, &message, &(comp_list_temp->comp_result)) == -1)
                    {
                        fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                        log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }

                    /* log the deletion */
                    memset(log_buffer, 0, MAX_BUF_LEN);
                    snprintf(log_buffer, MAX_BUF_LEN, "CLIENT: Deleting file %s\n", comp_list_temp->comp_result.file_info.path);
                    if (log_function(log_buffer, dir_name, &comp_list_temp->comp_result) == -1)
                    {
                        fprintf(stderr, "CLIENT: Cannot log the deletion\n");
                    }
                }
                    
                comp_list_temp = comp_list_temp->next;
            }

            /* send done message to server */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_DONE;

            if (send_message(client_socket, &message, &comp_result_server) == -1)
            {
                fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
        }
        else
        {
            /* send same message to server */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_SAME;

            if (send_message(client_socket, &message, &comp_result_server) == -1)
            {
                fprintf(stderr, "CLIENT: Server is disconnected, terminating...\n");
                log_function("CLIENT: Server is disconnected, terminating...\n", dir_name, NULL);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
        }

        /* do checking */
        if (checking_func (&head_prev, &tail_prev, &head_cur, &tail_cur, &comp_list, &change_flag, dir_name) == -1)
        {
            if (errno != EINTR)
                error_print("checking_func");
            continue;
        }
        
    }   

    return 0;
}


int log_function (char *log_message, const char *client_path, const comp_result_t *comp_result)
{
    int log_fd;
    char buffer[MAX_BUF_LEN+3];
    char time_buffer[150];
    time_t current_time;
    char file_type[20];

    /* concatenate log file name with client path */
    snprintf(buffer, MAX_BUF_LEN, "%s/%s", client_path, log_file_name_client);

    /* open log file */
    if ((log_fd = open(buffer, O_WRONLY | O_APPEND | O_CREAT, 0777)) == -1)
    {
        error_print("open");
        return -1;
    }

    if (comp_result != NULL && comp_result->comp_result == FILE_ADDED)
    {
        current_time = comp_result->file_info.last_modified;
        /* convert current time to string */
        if (strftime(time_buffer, MAX_BUF_LEN, "%d.%m.%Y %H:%M:%S", localtime(&current_time)) == 0)
        {
            fprintf(stderr, "CLIENT: Cannot convert time to string\n");
            close(log_fd);
            return -1;
        }

        /* get file type */
        memset(file_type, 0, 20);
        if (comp_result->file_info.file_type == REGULAR_FILE_TYPE)
            snprintf(file_type, 20, "Regular file");
        else if (comp_result->file_info.file_type == DIRECTORY_TYPE)
            snprintf(file_type, 20, "Directory");
        else if (comp_result->file_info.file_type == FIFO_FILE_TYPE)
            snprintf(file_type, 20, "Fifo file");
        else
            snprintf(file_type, 20, "Unknown");

        snprintf(buffer, MAX_BUF_LEN+3, "\nFile %s is added\nFile type: %s, File size: %ld bytes, File access time: %s\n", comp_result->file_info.path, file_type, comp_result->file_info.file_size, time_buffer);

        /* write log message to log file */
        if (write(log_fd, buffer, strlen(buffer)) == -1)
        {
            error_print("write");
            close(log_fd);
            return -1;
        }

        close(log_fd);
        return 0;
    }

    else if (comp_result != NULL && comp_result->comp_result == FILE_MODIFIED)
    {
        current_time = comp_result->file_info.last_modified;
        /* convert current time to string */
        if (strftime(time_buffer, MAX_BUF_LEN, "%d.%m.%Y %H:%M:%S", localtime(&current_time)) == 0)
        {
            fprintf(stderr, "CLIENT: Cannot convert time to string\n");
            close(log_fd);
            return -1;
        }

        if (comp_result->file_info.file_type == REGULAR_FILE_TYPE)
            snprintf(file_type, 20, "Regular file");
        else if (comp_result->file_info.file_type == DIRECTORY_TYPE)
            snprintf(file_type, 20, "Directory");
        else if (comp_result->file_info.file_type == FIFO_FILE_TYPE)
            snprintf(file_type, 20, "Fifo file");
        else
            snprintf(file_type, 20, "Unknown");

        snprintf(buffer, MAX_BUF_LEN+3, "\nFile %s is modified\nFile type: %s, File size: %ld bytes, File access time: %s\n", comp_result->file_info.path, file_type, comp_result->file_info.file_size, time_buffer);
        
        /* write log message to log file */
        if (write(log_fd, buffer, strlen(buffer)) == -1)
        {
            error_print("write");
            close(log_fd);
            return -1;
        }

        close(log_fd);
        return 0;
    }


    /* get current time */
    if ((current_time = time(NULL)) == -1)
    {
        error_print("time");
        close(log_fd);
        return -1;
    }

    /* convert current time to string */
    if (strftime(time_buffer, MAX_BUF_LEN, "%d.%m.%Y %H:%M:%S", localtime(&current_time)) == 0)
    {
        fprintf(stderr, "CLIENT: Cannot convert time to string\n");
        close(log_fd);
        return -1;
    }

    if (log_message == NULL)
    {
        fprintf(stderr, "CLIENT: Cannot concatenate time and log message\n");
        close(log_fd);
        return -1;
    }

    /* concatenate time and log message */
    snprintf(buffer, MAX_BUF_LEN+3, "\n%s : %s", time_buffer, log_message);

    /* write log message to log file */
    if (write(log_fd, buffer, strlen(buffer)) == -1)
    {
        error_print("write");
        close(log_fd);
        return -1;
    }

    close(log_fd);
    return 0;
}