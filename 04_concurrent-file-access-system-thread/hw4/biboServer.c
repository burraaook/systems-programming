#include "include/mycommon.h"
#include "include/bserver.h"
#include "include/thread_pool.h"

const char *DEFAULT_DIR_NAME = "Here";
char dir_name[MAX_FILENAME_LEN];

sig_atomic_t signal_occured = 0;

void sig_handler (int signal_number)
{
    signal_occured = signal_number;
}

void cleanup_and_terminate (const char *fifo_name);
void set_signal_flag (int flag);

int main (int argc, char *argv[])
{
    char cl_fifo_name[MAX_FILENAME_LEN];
    char sv_fifo_name[MAX_FILENAME_LEN];
    char log_file_name[MAX_FILENAME_LEN];
    char buffer[MAX_BUF_LEN];
    int flag, cl_fifo_fd, sv_fifo_fd;
    size_t max_clients, thread_pool_size;
    request_t request;
    response_t response;
    pid_t pid;
    struct sigaction sa;

    pid = getpid();
    request.sv_pid = pid;
    request.client_pid = 0;
    
    /* number of arguments is 4 */
    switch (argc)
    {
        case 3:
            /* get the max number of clients */
            max_clients = string_to_uint(argv[1], strlen(argv[1]));
            
            /* get the thread pool size */
            thread_pool_size = string_to_uint(argv[2], strlen(argv[2]));

            /* get the directory name */
            strcpy(dir_name, DEFAULT_DIR_NAME);
            break;
        case 4:
            /* get the directory name */
            strcpy(dir_name, argv[1]);

            /* get the number of clients */
            max_clients = string_to_uint(argv[2], strlen(argv[2]));

            /* get the thread pool size */
            thread_pool_size = string_to_uint(argv[3], strlen(argv[3]));
            break;

        default:
            error_exit_custom("./biboServer <dir_name> <max_clients> <thread_pool_size>");
    }

    /* set printf to be unbuffered */
    setbuf(stdout, NULL);

    /* set the signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_handler;
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    snprintf(log_file_name, MAX_FILENAME_LEN, LOG_FILE_NAME, pid);

    /* create the directory */
    while (((flag = mkdir(dir_name, 0777)) == -1) && errno == EINTR);
    if (flag == -1)
    {
        if (errno != EEXIST)
            error_exit("mkdir");
    }

    snprintf(cl_fifo_name, MAX_FILENAME_LEN, "%s_%d", CL_FIFO_NAME, pid);
    /* create fifo for client connection requests */
    while (((flag = mkfifo(cl_fifo_name, FIFO_PERM)) == -1) && errno == EINTR);
    if (flag == -1)
    {
        if (errno != EEXIST)
            error_exit("mkfifo");
    }

    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: >> Server started PID %d ..\n", pid);
    log_msg(log_file_name, buffer);

    /* initialize the thread pool */
    if (thread_pool_init(thread_pool_size) == -1)
        error_exit_custom("thread_pool_init");
    
    fprintf (stdout, ">> Server started PID %d ..\n", pid);
    fprintf (stdout, ">> Waiting for clients ..\n");
    while (1)
    {
        if (signal_occured > 0)
        {
            switch (signal_occured)
            {
                case SIGINT:
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> SIGINT received. Terminating..\n", pid);
                    log_msg(log_file_name, buffer);
                    cleanup_and_terminate(cl_fifo_name);
                break;

                case SIGTERM: 
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> SIGTERM received. Terminating..\n", pid);
                    log_msg(log_file_name, buffer);
                    cleanup_and_terminate(cl_fifo_name);
                break;

                case SIGQUIT:
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> SIGQUIT received. Terminating..\n", pid);
                    log_msg(log_file_name, buffer);
                    cleanup_and_terminate(cl_fifo_name);
                break;    
            }

            signal_occured = 0;
        }

        if (get_num_clients() < max_clients && get_num_clients_w() > 0)
        {
            /* poll the client */
            if (poll_client_w(&request.client_pid) == -1)
            {
                error_print_custom("poll_client_w");
                continue;
            }
        
            /* add the client to the queue */
            if (offer_client(request.client_pid) == -1)
            {
                error_print_custom("client cannot be added to the queue");
                continue;
            }
            fprintf(stdout, ">> Client PID %d connected\n", request.client_pid);
            snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Client PID %d connected\n", pid, request.client_pid);
            log_msg(log_file_name, buffer);

            /* open fifo file for sending the response */
            snprintf(sv_fifo_name, MAX_FILENAME_LEN, "%s_%d", SV_FIFO_NAME, request.client_pid);
            sv_fifo_fd = open(sv_fifo_name, O_WRONLY);
            if (sv_fifo_fd == -1)
            {
                error_print("open fifo1");
                continue;
            }

            block_all_signals();
            /* send the response */
            response.sv_pid = pid;
            if (write(sv_fifo_fd, &response, sizeof(struct response_t)) == -1)
            {
                error_print("write fifo");
                close(sv_fifo_fd);
                unblock_all_signals();
                continue;
            }
            unblock_all_signals();

            /* close the fifo */
            if (close(sv_fifo_fd) == -1)
                error_print("close");
        }

        /* print the queue */
        /* print_queue(); */
        /* print_queue_w(); */
        
        set_signal_flag(0);
        /* receive a connection request */    
        cl_fifo_fd = open(cl_fifo_name, O_RDONLY);
        if (cl_fifo_fd == -1)
        {
            if (errno == EINTR)
                cleanup_and_terminate(cl_fifo_name);
            else
            {
                error_print("open fifo2");
                continue;
            }
        }
        set_signal_flag(SA_RESTART);

        /* read the request */
        if (read(cl_fifo_fd, &request, sizeof(struct request_t)) == -1)
        {
            error_print("read fifo");
            close(cl_fifo_fd);
            continue;
        }

        /* fprintf(stdout, "> Received request from client %d\n", request.client_pid); */
        
        if (request.command == CMD_CONNECT || request.command == CMD_TRYCONNECT)
        {
            if (get_num_clients() < max_clients)
            {
                /* set the response */
                response.flag = SV_SUCCESS;

                /* add the client to the queue */
                if (offer_client(request.client_pid) == -1)
                {
                    error_print_custom("client cannot be added to the queue");
                    continue;
                }
                
                fprintf(stdout, ">> Client PID %d connected\n", request.client_pid);
                snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Client PID %d connected\n", pid, request.client_pid);
                log_msg(log_file_name, buffer);

                /* open fifo file for sending the response */
                snprintf(sv_fifo_name, MAX_FILENAME_LEN, "%s_%d", SV_FIFO_NAME, request.client_pid);
                sv_fifo_fd = open(sv_fifo_name, O_WRONLY);
                if (sv_fifo_fd == -1)
                {
                    error_print("open fifo3");
                    continue;
                }

                block_all_signals();
                /* send the response */
                response.sv_pid = pid;
                if (write(sv_fifo_fd, &response, sizeof(struct response_t)) == -1)
                {
                    error_print("write fifo");
                    close(sv_fifo_fd);
                    unblock_all_signals();
                    continue;
                }
                unblock_all_signals();

                /* close the fifo */
                if (close(sv_fifo_fd) == -1)
                    error_print("close");
            }

            else
            {
                /* set the response */
                response.flag = SV_FAILURE;
                response.sv_pid = 0;

                /* open fifo file for sending the response */
                snprintf(sv_fifo_name, MAX_FILENAME_LEN, "%s_%d", SV_FIFO_NAME, request.client_pid);
                sv_fifo_fd = open(sv_fifo_name, O_WRONLY);
                if (sv_fifo_fd == -1)
                {
                    error_print("open fifo4");
                    continue;
                }

                block_all_signals();
                /* send the response */
                if (write(sv_fifo_fd, &response, sizeof(struct response_t)) == -1)
                {
                    error_print("write fifo");
                    close(sv_fifo_fd);
                    unblock_all_signals();
                    continue;
                }
                unblock_all_signals();

                /* close the fifo */
                if (close(sv_fifo_fd) == -1)
                    error_print("close");

                /* add the client to the queue */
                if (request.command != CMD_TRYCONNECT)
                {
                    offer_client_w(request.client_pid);
                    fprintf(stdout, ">> Connection request PID %d.. QUE FULL\n", request.client_pid);
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Connection request PID %d.. QUE FULL\n", pid, request.client_pid);
                    log_msg(log_file_name, buffer);
                }
            }
        }

        /* client wants to disconnect */
        else if (request.command == CMD_QUIT)
        {
            /* if it is waiting client */
            if (request.wait_flag == 1)
            {
                /* remove the client from the queue */
                remove_client_w(request.client_pid);
            }
            else
            {

                /* remove the client from the queue */
                remove_client(request.client_pid);

                fprintf(stdout, ">> Client PID %d disconnected\n", request.client_pid);
                snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Client PID %d disconnected\n", pid, request.client_pid);
                log_msg(log_file_name, buffer);
            }
        }
        
        else if (request.command == CMD_KILLSERVER)
        {
            fprintf(stdout, ">> Kill signal from client PID %d, terminating..\n", request.client_pid);
            snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Kill signal from client PID %d, terminating..\n", pid, request.client_pid);
            log_msg(log_file_name, buffer);
            /* terminate threads */
            cleanup_and_terminate(cl_fifo_name);
        }

        else
        {
            /* submit the task to the thread pool */
            if (thread_pool_submit(&request) == -1)
            {
                error_print_custom("thread_pool_submit");
                continue;
            }
        }
    }

    cleanup_and_terminate(cl_fifo_name);
    return 0;
}

void cleanup_and_terminate (const char *fifo_name)
{
    size_t num_clients, i, num_clients_w;
    pid_t client_pid;

    /* remove the fifo file */
    unlink(fifo_name);

    /* send sigint to all clients */
    num_clients = get_num_clients();
    for (i = 0; i < num_clients; i++)
    {
        if (poll_client(&client_pid) == -1)
            error_print_custom("poll_client");

        kill(client_pid, SIGINT);
    }

    /* send sigint to all waiting clients */
    num_clients_w = get_num_clients_w();
    for (i = 0; i < num_clients_w; i++)
    {
        if (poll_client_w(&client_pid) == -1)
            error_print_custom("poll_client_w");

        if (kill(client_pid, SIGINT) == -1)
            error_print("kill2");
    }

    fprintf(stdout, ">> bye..\n");

    thread_pool_destroy();
    exit(EXIT_SUCCESS);
}

void set_signal_flag (int flag)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &sig_handler;
    sa.sa_flags = flag;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}