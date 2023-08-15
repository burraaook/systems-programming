#include "include/mycommon.h"
#include "include/bserver.h"

const char *DEFAULT_DIR_NAME = "Here";

sig_atomic_t signal_occured = 0;

void sig_handler (int signal_number)
{
    signal_occured = signal_number;
}

int add_child (pid_t child_pid, pid_t **child_pids, size_t *num_child, size_t *cap);
void cleanup_and_terminate (const char *fifo_name, pid_t *child_pids, size_t num_child, sem_t *log_sem, const char *log_sem_name,
                            sem_t *write_sem, const char *write_sem_name);
void free_childs (pid_t *child_pids);
void set_signal_flag (int flag);
void exec_child_sv (pid_t client_pid, const char* dir_name);
int remove_child (pid_t child_pid, pid_t **child_pids, size_t *num_child);
void log_msg (const char *log_file_name, const char *msg, sem_t *log_sem);



int main (int argc, char *argv[])
{
    char dir_name[MAX_FILENAME_LEN];
    char fifo_name[MAX_FILENAME_LEN];
    char cl_fifo_name[MAX_FILENAME_LEN];
    char log_file_name[MAX_FILENAME_LEN];
    char buffer[MAX_BUF_LEN];
    char write_sem_name[MAX_FILENAME_LEN];
    int flag, fifoFd, val;
    size_t max_clients;
    request_t request;
    response_t response;
    pid_t child_pid, pid;
    pid_t *child_pids;
    size_t num_child, cap;
    char log_sem_name[MAX_FILENAME_LEN];

    sem_t *log_sem, *write_sem;
    struct sigaction sa;

    num_child = 0;
    request.pid = 0;
    request.sv_pid = 0;
    cap = 10;

    pid = getpid();

    /* allocate memory for child pids */
    if ((child_pids = malloc(cap * sizeof(pid_t))) == NULL)
        error_exit("malloc");

    /* number of arguments is 3 */
    switch (argc)
    {
        case 2:
            /* get the max number of clients */
            max_clients = string_to_uint(argv[1], strlen(argv[1]));
            
            /* get the directory name */
            strcpy(dir_name, DEFAULT_DIR_NAME);
            break;
        case 3:
            /* get the directory name */
            strcpy(dir_name, argv[1]);

            /* get the number of clients */
            max_clients = string_to_uint(argv[2], strlen(argv[2]));
            break;

        default:
            error_exit_custom("./biboServer <dir_name> <max_clients>");
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
    sigaction(SIGCHLD, &sa, NULL);

    snprintf(log_file_name, MAX_FILENAME_LEN, LOG_FILE_NAME, pid);

    /* create log file semaphore */
    snprintf(log_sem_name, MAX_FILENAME_LEN, LOG_SEM_NAME, pid);

    /* create the semaphore */
    if ((log_sem = sem_open(log_sem_name, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED)
    {
        if (errno != EEXIST)
        {
            /* check its value */
            sem_getvalue(log_sem, &val);
            if (val == 0)
                sem_post(log_sem);
        }
    }

    /* create write semaphore */
    snprintf(write_sem_name, MAX_FILENAME_LEN, WRITE_SEM_NAME, pid);

    /*    fprintf(stderr, "write_sem_name: %s\n", write_sem_name); */

    /* create the semaphore */
    if ((write_sem = sem_open(write_sem_name, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED)
    {
        if (errno != EEXIST)
        {
            /* check its value */
            sem_getvalue(write_sem, &val);
            if (val == 0)
                sem_post(write_sem);
        }
    }

    /* create the directory */
    while (((flag = mkdir(dir_name, 0777)) == -1) && errno == EINTR);
    if (flag == -1)
    {
        if (errno != EEXIST)
            error_exit("mkdir");
    }

    snprintf(fifo_name, MAX_FILENAME_LEN, "%s_%d", FIFO_NAME, pid);

    /* create fifo for client connection requests */
    while (((flag = mkfifo(fifo_name, FIFO_PERM)) == -1) && errno == EINTR);
    if (flag == -1)
    {
        if (errno != EEXIST)
            error_exit("mkfifo");
    }

    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: >> Server started PID %d ..\n", pid);
    log_msg(log_file_name, buffer, log_sem);

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
                    log_msg(log_file_name, buffer, log_sem);
                    cleanup_and_terminate(fifo_name, child_pids, num_child, log_sem, log_sem_name, write_sem, write_sem_name);
                break;

                case SIGTERM: 
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> SIGTERM received. Terminating..\n", pid);
                    log_msg(log_file_name, buffer, log_sem);
                    cleanup_and_terminate(fifo_name, child_pids, num_child, log_sem, log_sem_name, write_sem, write_sem_name);
                break;

                case SIGQUIT:
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> SIGQUIT received. Terminating..\n", pid);
                    log_msg(log_file_name, buffer, log_sem);
                    cleanup_and_terminate(fifo_name, child_pids, num_child, log_sem, log_sem_name, write_sem, write_sem_name);
                break;

                case SIGCHLD:
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> SIGCHLD received.\n", pid);
                    log_msg(log_file_name, buffer, log_sem);
                break;
                
            }

            signal_occured = 0;
        }

        /* print the queue */
        /* print_queue(); */
        /* print_queue_w(); */

        if (get_num_clients() < max_clients && get_num_clients_w() > 0)
        {
            /* poll the client */
            if (poll_client_w(&request.pid) == -1)
            {
                error_print_custom("poll_client_w");
                continue;
            }

            /* child server for the client */
            child_pid = fork();
            if (child_pid == 0)
            {
                exec_child_sv(request.pid, dir_name);
            }
            else if (child_pid == -1)
            {
                error_print("fork");
                continue;
            }
            else
            {
                /* parent */
                /* add the child pid to the array */           
                add_child(child_pid, &child_pids, &num_child, &cap);
            
                /* add the client to the queue */
                if (offer_client(request.pid) == -1)
                {
                    error_print_custom("client cannot be added to the queue");
                    continue;
                }

                fprintf(stdout, ">> Client PID %d connected\n", request.pid);
                snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Client PID %d connected\n", pid, request.pid);
                log_msg(log_file_name, buffer, log_sem);

                /* open fifo file for sending the response */
                snprintf(cl_fifo_name, MAX_FILENAME_LEN, "%s_%d", CL_FIFO_NAME, request.pid);
                fifoFd = open(cl_fifo_name, O_WRONLY);
                if (fifoFd == -1)
                {
                    error_print("open fifo1");
                    continue;
                }

                block_all_signals();
                /* send the response */
                response.sv_pid = child_pid;
                if (write(fifoFd, &response, sizeof(struct response_t)) == -1)
                {
                    error_print("write fifo");
                    close(fifoFd);
                    unblock_all_signals();
                    continue;
                }
                unblock_all_signals();

                /* close the fifo */
                if (close(fifoFd) == -1)
                    error_print("close");
            }
        }
        
        set_signal_flag(0);
        /* receive a connection request */    
        fifoFd = open(fifo_name, O_RDONLY);
        if (fifoFd == -1)
        {
            if (errno == EINTR)
                cleanup_and_terminate(fifo_name, child_pids, num_child, log_sem, log_sem_name, write_sem, write_sem_name);
            else
            {
                error_print("open fifo2");
                continue;
            }
        }
        set_signal_flag(SA_RESTART);

        /* read the request */
        if (read(fifoFd, &request, sizeof(struct request_t)) == -1)
        {
            error_print("read fifo");
            close(fifoFd);
            continue;
        }

        /* fprintf(stdout, "> Received request from client %d\n", request.pid); */
        
        if (request.command == CMD_CONNECT || request.command == CMD_TRYCONNECT)
        {
            if (get_num_clients() < max_clients)
            {
                /* set the response */
                response.flag = SV_SUCCESS;
                child_pid = fork();

                /* child server for the client */
                if (child_pid == 0)
                {
                    /* close the fifo */
                    if (close(fifoFd) == -1)
                        error_print("close");

                    exec_child_sv(request.pid, dir_name);
                }
                else if (child_pid == -1)
                {
                    error_print("fork");
                }
                else
                {
                    /* parent */
                    add_child(child_pid, &child_pids, &num_child, &cap);

                    /* add the client to the queue */
                    if (offer_client(request.pid) == -1)
                    {
                        error_print_custom("client cannot be added to the queue");
                        continue;
                    }
                    
                    fprintf(stdout, ">> Client PID %d connected\n", request.pid);
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Client PID %d connected\n", pid, request.pid);
                    log_msg(log_file_name, buffer, log_sem);

                    /* open fifo file for sending the response */
                    snprintf(cl_fifo_name, MAX_FILENAME_LEN, "%s_%d", CL_FIFO_NAME, request.pid);
                    fifoFd = open(cl_fifo_name, O_WRONLY);
                    if (fifoFd == -1)
                    {
                        error_print("open fifo3");
                        continue;
                    }

                    block_all_signals();
                    /* send the response */
                    response.sv_pid = child_pid;
                    if (write(fifoFd, &response, sizeof(struct response_t)) == -1)
                    {
                        error_print("write fifo");
                        close(fifoFd);
                        unblock_all_signals();
                        continue;
                    }
                    unblock_all_signals();

                    /* close the fifo */
                    if (close(fifoFd) == -1)
                        error_print("close");
                }
            }

            else
            {
                /* set the response */
                response.flag = SV_FAILURE;
                response.sv_pid = 0;

                /* open fifo file for sending the response */
                snprintf(cl_fifo_name, MAX_FILENAME_LEN, "%s_%d", CL_FIFO_NAME, request.pid);
                fifoFd = open(cl_fifo_name, O_WRONLY);
                if (fifoFd == -1)
                {
                    error_print("open fifo4");
                    continue;
                }

                block_all_signals();
                /* send the response */
                if (write(fifoFd, &response, sizeof(struct response_t)) == -1)
                {
                    error_print("write fifo");
                    close(fifoFd);
                    unblock_all_signals();
                    continue;
                }
                unblock_all_signals();

                /* close the fifo */
                if (close(fifoFd) == -1)
                    error_print("close");

                /* add the client to the queue */
                if (request.command != CMD_TRYCONNECT)
                {
                    offer_client_w(request.pid);
                    fprintf(stdout, ">> Connection request PID %d.. QUE FULL\n", request.pid);
                    snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Connection request PID %d.. QUE FULL\n", pid, request.pid);
                    log_msg(log_file_name, buffer, log_sem);
                }
            }
        }

        /* client wants to disconnect */
        else if (request.command == CMD_QUIT)
        {
            /* if it is waiting client */
            if (request.sv_pid == 0)
            {
                /* remove the client from the queue */
                remove_client_w(request.pid);
            }
            else
            {
                /* remove child server process */
                if (kill(request.sv_pid, SIGINT) == -1)
                    error_print("kill");
                /* remove the client from the queue */
                remove_client(request.pid);
                /* remove child process from the array */
                remove_child(request.sv_pid, &child_pids, &num_child);
                waitpid(request.sv_pid, NULL, 0);

                fprintf(stdout, ">> Client PID %d disconnected\n", request.pid);
                snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Client PID %d disconnected\n", pid, request.pid);
                log_msg(log_file_name, buffer, log_sem);
            }
        }
        
        else if (request.command == CMD_KILLSERVER)
        {
            fprintf(stdout, ">> Kill signal from client PID %d, terminating..\n", request.pid);
            snprintf(buffer, MAX_BUF_LEN, "MAIN SERVER: %d >> Kill signal from client PID %d, terminating..\n", pid, request.pid);
            log_msg(log_file_name, buffer, log_sem);
            cleanup_and_terminate(fifo_name, child_pids, num_child, log_sem, log_sem_name, write_sem, write_sem_name);
        }
    }

    cleanup_and_terminate(fifo_name, child_pids, num_child, log_sem, log_sem_name, write_sem, write_sem_name);
    return 0;
}

void cleanup_and_terminate (const char *fifo_name, pid_t *child_pids, size_t num_child, sem_t *log_sem, const char *log_sem_name,
                            sem_t *write_sem, const char *write_sem_name)
{
    size_t num_clients, i, num_clients_w;
    pid_t client_pid;

    /* remove the fifo file */
    unlink(fifo_name);

    /* send sigint to all children, and clients */
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

    for (i = 0; i < num_child; i++)
    {        
        if (kill(child_pids[i], SIGINT) == -1)
            error_print("kill3");
        waitpid(child_pids[i], NULL, 0);
    }

    fprintf(stdout, ">> bye..\n");

    /* close the log semaphore */
    if (sem_close(log_sem) == -1)
        error_print("sem_close");
    
    /* remove the log semaphore */
    if (sem_unlink(log_sem_name) == -1)
        error_print("sem_unlink");

    /* close the write semaphore */
    if (sem_close(write_sem) == -1)
        error_print("sem_close");

    /* remove the write semaphore */
    if (sem_unlink(write_sem_name) == -1)
        error_print("sem_unlink");

    free_childs(child_pids);
    exit(EXIT_SUCCESS);
}

int add_child (pid_t child_pid, pid_t **child_pids, size_t *num_child, size_t *cap)
{
    if (*num_child == *cap)
    {
        *cap *= 2;
        *child_pids = realloc(*child_pids, *cap * sizeof(pid_t));
        if (*child_pids == NULL)
        {
            error_print("realloc");
            return -1;
        }
    }

    (*child_pids)[*num_child] = child_pid;
    (*num_child)++;

    return 0;
}

int remove_child (pid_t child_pid, pid_t **child_pids, size_t *num_child)
{
    size_t i, j;

    for (i = 0; i < *num_child; i++)
    {
        if ((*child_pids)[i] == child_pid)
            break;
    }

    if (i == *num_child)
        return -1;

    for (j = i; j < *num_child - 1; j++)
        (*child_pids)[j] = (*child_pids)[j + 1];

    (*num_child)--;

    return 0;
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

void free_childs (pid_t *child_pids)
{
    free(child_pids);
}

void exec_child_sv (pid_t client_pid, const char* dir_name)
{
    char client_pid_str[20];
    char *args[4];
    char buffer[MAX_FILENAME_LEN];

    strcpy(buffer, dir_name);

    /* convert client_pid to string */
    snprintf(client_pid_str, sizeof(client_pid_str), "%d", client_pid);

    /* set command-line arguments */
    args[0] = "./src/child_server";
    args[1] = client_pid_str;
    args[2] = buffer;
    args[3] = NULL;

    /* execute child server */
    execvp(args[0], args);
    error_exit("execvp");
}

void log_msg (const char *log_file_name, const char *msg, sem_t *log_sem)
{
    int log_fd;

    /* lock the semaphore */
    if (sem_wait(log_sem) == -1)
        error_print("sem_wait");

    /* open the log file */
    log_fd = open(log_file_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1)
        error_exit("open log");


    block_all_signals();
    /* write the message to the log file */
    if (write(log_fd, msg, strlen(msg)) == -1)
        error_exit("write log");
    unblock_all_signals();

    /* close the log file */
    if (close(log_fd) == -1)
        error_exit("close log");

    /* unlock the semaphore */
    if (sem_post(log_sem) == -1)
        error_print("sem_post");
}