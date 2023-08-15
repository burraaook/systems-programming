#include "../include/thread_pool.h"

thread_pool_t thread_pool;

char dir_name[MAX_FILENAME_LEN];

int thread_pool_init (size_t num_threads)
{
    size_t i;

    if (num_threads <= 0)
    {
        error_print_custom("Invalid number of threads");
        return -1;
    }

    thread_pool.num_threads = num_threads;
    thread_pool.num_threads_working = 0;
    thread_pool.shutdown = 0;

    if ((thread_pool.threads = malloc(num_threads * sizeof(pthread_t))) == NULL)
    {
        error_print("malloc");
        return -1;
    }

    /* initialize task queue */
    if (task_queue_init() == -1)
    {
        error_print("task_queue_init");
        free(thread_pool.threads);
        return -1;
    }

    /* initialize mutex and condition variables, default attributes */
    if (pthread_mutex_init(&(thread_pool.mutex), NULL) != 0)
    {
        error_print("pthread_mutex_init");
        free(thread_pool.threads);
        return -1;
    }

    if (pthread_cond_init(&(thread_pool.cond), NULL) != 0)
    {
        error_print("pthread_cond_init");
        free(thread_pool.threads);
        return -1;
    }

    /* initalize log_mutex */
    if (pthread_mutex_init(&(thread_pool.log_mutex), NULL) != 0)
    {
        error_print("pthread_mutex_init");
        free(thread_pool.threads);
        return -1;
    }

    /* initialize read write mutex */
    if (pthread_rwlock_init(&(thread_pool.rwlock), NULL) != 0)
    {
        error_print("pthread_rwlock_init");
        free(thread_pool.threads);
        return -1;
    }

    /* create threads */
    for (i = 0; i < num_threads; ++i)
    {
        if (pthread_create(&(thread_pool.threads[i]), NULL, worker_thread, NULL) != 0)
        {
            error_print("pthread_create");
            free(thread_pool.threads);
            return -1;
        }
    }

    return 0;
}

int thread_pool_destroy ()
{
    size_t i;

    /* set shutdown flag to 1 and broadcast to wake up all worker threads */
    if (pthread_mutex_lock(&(thread_pool.mutex)) != 0)
    {
        error_print("pthread_mutex_lock");
        free(thread_pool.threads);
        return -1;
    }

    thread_pool.shutdown = 1;

    if (pthread_cond_broadcast(&(thread_pool.cond)) != 0)
    {
        error_print("pthread_cond_broadcast");
        free(thread_pool.threads);
        return -1;
    }

    if (pthread_mutex_unlock(&(thread_pool.mutex)) != 0)
    {
        error_print("pthread_mutex_unlock");
        free(thread_pool.threads);
        return -1;
    }

    /* wait for all worker threads to exit */
    for (i = 0; i < thread_pool.num_threads; ++i)
    {
        if (thread_pool.num_threads_working > 0)
        {
            /* directly terminate the thread */
            if (pthread_cancel(thread_pool.threads[i]) != 0)
            {
                error_print("pthread_cancel");
                free(thread_pool.threads);
                return -1;
            }
        }
        else if (pthread_join(thread_pool.threads[i], NULL) != 0)
        {
            error_print("pthread_join");
            free(thread_pool.threads);
            return -1;
        }
    }

    /* destroy mutex and condition variables */
    pthread_mutex_destroy(&(thread_pool.mutex));
    pthread_cond_destroy(&(thread_pool.cond));
    pthread_mutex_destroy(&(thread_pool.log_mutex));
    pthread_rwlock_destroy(&(thread_pool.rwlock));

    /* free memory */
    free(thread_pool.threads);

    /* destroy task queue */
    if (task_queue_destroy() == -1)
    {
        error_print("task_queue_destroy");
        free(thread_pool.threads);
        return -1;
    }

    return 0;
}

int thread_pool_submit (const request_t *task)
{
    if (task == NULL)
    {
        error_print_custom("Invalid task");
        return -1;
    }

    /* add task to queue */
    if (offer_task(task) == -1)
    {
        error_print("offer_task");
        return -1;
    }
    
    /* signal worker thread */
    if (pthread_cond_signal(&(thread_pool.cond)) != 0)
    {
        error_print("pthread_cond_signal");
        return -1;
    }

    return 0;
}

int thread_pool_get_size ()
{
    return thread_pool.num_threads;
}

int thread_pool_get_num_threads_working ()
{
    return thread_pool.num_threads_working;
}

void *worker_thread (void *args)
{
    request_t task;

    /* for preventing unused parameter warning */
    (void) args;

    while (1)
    {
        /* lock mutex and wait for signal */
        if (pthread_mutex_lock(&(thread_pool.mutex)) != 0)
        {
            error_print("pthread_mutex_lock");
            return NULL;
        }

        while (task_queue_get_size() == 0 && !thread_pool.shutdown)
        {
            if (pthread_cond_wait(&(thread_pool.cond), &(thread_pool.mutex)) != 0)
            {
                error_print("pthread_cond_wait");
                return NULL;
            }
        }

        /* shutdown */
        if (thread_pool.shutdown)
        {
            if (pthread_mutex_unlock(&(thread_pool.mutex)) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }

            pthread_exit(NULL);
        }

        /* grab task */
        if (poll_task(&task) == -1)
        {
            error_print("poll_task");
            return NULL;
        }

        /* increment number of threads working */
        thread_pool.num_threads_working++;

        /* unlock mutex */
        if (pthread_mutex_unlock(&(thread_pool.mutex)) != 0)
        {
            error_print("pthread_mutex_unlock");
            return NULL;
        }

        block_all_signals();
        /* execute task */
        execute_task(&task);
        unblock_all_signals();

        /* decrement number of threads working */
        thread_pool.num_threads_working--;
    }

    return NULL;
}

int execute_task (const request_t *task)
{
    char sv_fifo_name[MAX_FILENAME_LEN];
    char log_file_name[MAX_FILENAME_LEN];
    char file_name[MAX_FILENAME_LEN];
    char shm_name[MAX_FILENAME_LEN];
    char buffer[MAX_BUF_LEN];
    response_t response;
    int file_fd;
    pid_t pid, client_pid;
    char *message;
    response.flag = SV_SUCCESS;
    pid = getpid();
    client_pid = task->client_pid;

    /* get log file name */
    snprintf(log_file_name, MAX_FILENAME_LEN, LOG_FILE_NAME, pid);

    /* set printf to be unbuffered */
    setbuf(stdout, NULL);

    /* sv fifo name for sending response */
    snprintf(sv_fifo_name, MAX_FILENAME_LEN, "%s_%d", SV_FIFO_NAME, client_pid);

    /* open/create shm */
    snprintf(shm_name, MAX_FILENAME_LEN, "%s_%d", SHM_NAME, client_pid);

    snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> Thread for client %d is activated\n", client_pid);
    log_msg(log_file_name, buffer);

    switch (task->command)
    {
        case CMD_HELP:

            message = malloc(sizeof(char) * MAX_MESSAGE_LEN);

            if (message == NULL)
            {
                error_print("malloc");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            snprintf(message, MAX_MESSAGE_LEN,
            "\n   Available Comments are:\nconnect, tryConnect, help, list, readF, writeT, upload, download, quit, killServer\n\n");
            response.file_size = strlen(message) + 1;                    

            if (write_message_shm (shm_name, message, strlen(message) + 1) == -1)
            {
                error_print("write_message_shm");
                free(message);
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print("send_response");
                free(message);
                return -1;
            }

            free(message);

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> help command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);
        break;

        case CMD_READF:
            response.flag = SV_SUCCESS;

            /* get file name */
            snprintf(file_name, MAX_FILENAME_LEN + 1, "%s/", dir_name);
            strncat(file_name, task->file_name, MAX_FILENAME_LEN - strlen(file_name));
            
            /* open file */
            if ((file_fd = open(file_name, O_RDONLY)) == -1)
                response.flag = SV_FAILURE;

            /* enter critical section for read */
            if (pthread_rwlock_rdlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_rdlock");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            if (response.flag != SV_FAILURE)
            {
                if (write_line_to_shm(file_fd, shm_name, 
                        &response.file_size, task->arg1) == -1)
                {
                    error_print_custom("readF failed");
                    response.flag = SV_FAILURE;
                }
            }
            
            /* exit critical section */
            if (pthread_rwlock_unlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_unlock");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            /* send response */
            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print_custom("send_response failed");
                return -1;
            }

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> readF command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);
        break;
    
        case CMD_LIST:

            response.flag = SV_SUCCESS;

            /* get file list */
            if (get_file_list_shm(shm_name, &response.file_size, dir_name) == -1)
            {
                error_print_custom("get_file_list_shm failed");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
            }

            /* send response to the client */
            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print_custom("send_response failed");
                return -1;
            }

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> list command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);
        break;

        case CMD_UPLOAD:

            response.flag = SV_SUCCESS;

            /* get file name */
            snprintf(file_name, MAX_FILENAME_LEN+1, "%s/", dir_name);
            strncat(file_name, task->file_name, MAX_FILENAME_LEN - strlen(file_name));
            
            /* critical section for write */
            if (pthread_rwlock_wrlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_wrlock");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            /* read from shared memory, write to file */
            if (write_shm_to_file(file_name, shm_name, task->file_size) == -1)
            {
                error_print_custom("write_shm_to_file failed");
                response.flag = SV_FAILURE;
            }

            /* exit critical section */
            if (pthread_rwlock_unlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_unlock");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            /* send response to the client */
            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print_custom("send_response failed");
                return -1;
            }

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> upload command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);        
        break;

        case CMD_DOWNLOAD:

            /* get file name */
            snprintf(file_name, MAX_FILENAME_LEN+1, "%s/", dir_name);
            strncat(file_name, task->file_name, MAX_FILENAME_LEN - strlen(file_name));
            response.flag = SV_SUCCESS;

            /* open file */
            if ((file_fd = open(file_name, O_RDONLY)) == -1)
            {
                error_print_custom("open failed");
                response.flag = SV_FAILURE;

                /* send response to the client */
                send_response(sv_fifo_name, response);
                return -1;
            }

            /* get file size */
            response.file_size = lseek(file_fd, 0, SEEK_END);
            lseek(file_fd, 0, SEEK_SET);

            /* enter critical section for read */
            if (pthread_rwlock_rdlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_rdlock");
                response.flag = SV_FAILURE;
                close(file_fd);
                send_response(sv_fifo_name, response);
                return -1;
            }

            if (write_file_to_shm(file_fd, shm_name, response.file_size) == -1)
            {
                error_print_custom("write_file_to_shm failed");
                response.flag = SV_FAILURE;
                close(file_fd);
                send_response(sv_fifo_name, response);
                pthread_rwlock_unlock(&(thread_pool.rwlock));
                return -1;
            }

            /* exit critical section */
            if (pthread_rwlock_unlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_unlock");
                response.flag = SV_FAILURE;
                close(file_fd);
                send_response(sv_fifo_name, response);
                return -1;
            }

            /* send response to the client */
            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print_custom("send_response failed");
                return -1;
            }

            close(file_fd);

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> download command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);
        break;

        case CMD_WRITET:
            
            response.flag = SV_SUCCESS;

            /* get file name */
            snprintf(file_name, MAX_FILENAME_LEN+1, "%s/", dir_name);
            strncat(file_name, task->file_name, MAX_FILENAME_LEN - strlen(file_name));

            /* open file, create if not exists */
            if ((file_fd = open(file_name, O_RDWR | O_CREAT, 0666)) == -1)
            {
                error_print("open1");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            /* enter critical section for write */
            if (pthread_rwlock_wrlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_wrlock");
                response.flag = SV_FAILURE;
                close(file_fd);
                send_response(sv_fifo_name, response);
                return -1;
            }

            if (write_nth_line(file_fd, task->arg1, task->message, shm_name) == -1)
            {
                fprintf(stderr, "writeT failed\n");
                response.flag = SV_FAILURE;
                close(file_fd);
                send_response(sv_fifo_name, response);
                pthread_rwlock_unlock(&(thread_pool.rwlock));
                return -1;
            }

            /* exit critical section */
            if (pthread_rwlock_unlock(&(thread_pool.rwlock)) != 0)
            {
                error_print("pthread_rwlock_unlock");
                response.flag = SV_FAILURE;
                close(file_fd);
                send_response(sv_fifo_name, response);
                return -1;
            }
            close(file_fd);

            /* send response to the client */
            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print("send_response");
                return -1;
            }

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> writeT command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);
        break;

        default:

            if (task->command > 16)
            {
                fprintf(stderr, "Unknown command\n");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            response.flag = SV_SUCCESS;
            message = malloc (sizeof(char) * MAX_MESSAGE_LEN);

            if (message == NULL)
            {
                error_print("malloc");
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            if (get_help_message(message, task->command) == -1)
            {
                error_print_custom("get_help_message failed");
                free(message);
                response.flag = SV_FAILURE;
                send_response(sv_fifo_name, response);
                return -1;
            }

            if (write_message_shm (shm_name, message, strlen(message) + 1) == -1)
            {
                fprintf(stderr, "Help failed\n");
                free(message);
                response.flag = SV_FAILURE;     
                send_response(sv_fifo_name, response);
                return -1;
            }

            response.file_size = strlen(message) + 1;

            if (send_response(sv_fifo_name, response) == -1)
            {
                error_print_custom("send_response");
                free(message);
                return -1;
            }

            free(message);

            snprintf(buffer, MAX_BUF_LEN, "SERVER THREAD: >> help command is executed for client %d\n", client_pid);
            log_msg(log_file_name, buffer);
        break;
    }

    return 0;
}

void log_msg (const char *log_file_name, const char *msg)
{
    int log_fd;

    /* log mutex lock */
    if (pthread_mutex_lock(&(thread_pool.log_mutex)) != 0)
    {
        error_print("pthread_mutex_lock");
        return;
    }

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

    /* log mutex unlock */
    if (pthread_mutex_unlock(&(thread_pool.log_mutex)) != 0)
    {
        error_print("pthread_mutex_unlock");
        return;
    }
}