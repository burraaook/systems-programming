#include "../include/thread_pool.h"

thread_pool_t thread_pool;

int thread_pool_init (const size_t *num_consumers, producer_args_t *producer_args, consumer_args_t *consumer_args)
{
    size_t i;

    thread_pool.num_consumers = *num_consumers;
    thread_pool.num_consumers_working = 0;
    thread_pool.producer_done = 0;
    thread_pool.terminate = 0;

    block_all_signals();
    if ((thread_pool.consumer_threads = malloc(sizeof(pthread_t) * thread_pool.num_consumers)) == NULL)
    {
        error_print("malloc");
        return -1;
    }

    /* initalize task queue */
    if (task_queue_init() == -1)
    {
        error_print("task_queue_init");
        free(thread_pool.consumer_threads);
        unblock_all_signals();
        return -1;
    }

    /* initialize mutex and condition variable, default attributes */
    if (pthread_mutex_init(&thread_pool.mutex, NULL) != 0)
    {
        error_print("pthread_mutex_init");
        free(thread_pool.consumer_threads);
        unblock_all_signals();
        return -1;
    }

    /* initalize stdout mutex */
    if (pthread_mutex_init(&thread_pool.stdout_mutex, NULL) != 0)
    {
        error_print("pthread_mutex_init");
        free(thread_pool.consumer_threads);
        unblock_all_signals();
        return -1;
    }

    if (pthread_cond_init(&thread_pool.empty, NULL) != 0)
    {
        error_print("pthread_cond_init");
        free(thread_pool.consumer_threads);
        unblock_all_signals();
        return -1;
    }

    if (pthread_cond_init(&thread_pool.full, NULL) != 0)
    {
        error_print("pthread_cond_init");
        free(thread_pool.consumer_threads);
        unblock_all_signals();
        return -1;
    }
    unblock_all_signals();

    /* create producer thread */
    if (pthread_create(&thread_pool.producer_thread, NULL, producer_worker, producer_args) != 0)
    {
        error_print("pthread_create");
        free(thread_pool.consumer_threads);
        return -1;
    }

    /* create consumer threads */
    for (i = 0; i < thread_pool.num_consumers; ++i)
    {
        if (pthread_create(&thread_pool.consumer_threads[i], NULL, consumer_worker, consumer_args) != 0)
        {
            error_print("pthread_create");
            free(thread_pool.consumer_threads);
            return -1;
        }
    }

    return 0;
}

int thread_pool_destroy ( )
{
    size_t i;

    block_all_signals();

    /* wait for producer thread to finish */
    if (pthread_join(thread_pool.producer_thread, NULL) != 0)
    {
        error_print("pthread_join");
        return -1;
    }

    /* wait for consumer threads to finish */
    for (i = 0; i < thread_pool.num_consumers; ++i)
    {
        if (pthread_join(thread_pool.consumer_threads[i], NULL) != 0)
        {
            error_print("pthread_join");
            return -1;
        }
    }

    /* destroy mutex and condition variable */
    if (pthread_mutex_destroy(&thread_pool.mutex) != 0)
    {
        error_print("pthread_mutex_destroy");
        return -1;
    }

    /* destroy stdout mutex */
    if (pthread_mutex_destroy(&thread_pool.stdout_mutex) != 0)
    {
        error_print("pthread_mutex_destroy");
        return -1;
    }

    if (pthread_cond_destroy(&thread_pool.empty) != 0)
    {
        error_print("pthread_cond_destroy");
        return -1;
    }

    if (pthread_cond_destroy(&thread_pool.full) != 0)
    {
        error_print("pthread_cond_destroy");
        return -1;
    }

    /* destroy task queue */
    if (task_queue_destroy() == -1)
    {
        error_print("task_queue_destroy");
        return -1;
    }

    /* free consumer threads */
    free(thread_pool.consumer_threads);

    unblock_all_signals();

    return 0;
}

void *producer_worker (void *arg)
{
    char buffer[MAX_PATH_LEN];

    producer_args_t *args = (producer_args_t *) arg;

    while (1)
    {
        /* lock mutex and wait for signal */
        if (pthread_mutex_lock(&thread_pool.mutex) != 0)
        {
            error_print("pthread_mutex_lock");
            return NULL;
        }

        while (task_queue_get_size() >= args->buffer_size && thread_pool.terminate == 0)
        {
            if (pthread_cond_wait(&thread_pool.empty, &thread_pool.mutex) != 0)
            {
                error_print("pthread_cond_wait");
                return NULL;
            }
        }

        /* check if thread pool is terminating */
        if (thread_pool.terminate == 1)
        {
            if (pthread_mutex_unlock(&thread_pool.mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }

            thread_pool.producer_done = 1;

            /* signal consumer threads to terminate */
            if (pthread_cond_broadcast(&thread_pool.full) != 0)
            {
                error_print("pthread_cond_broadcast");
                return NULL;
            }

            /* lock stdout mutex */
            if (pthread_mutex_lock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_lock");
                return NULL;
            }
            fprintf(stderr, "PRODUCER: Termination signal received, terminating..\n");
            
            /* unlock stdout mutex */
            if (pthread_mutex_unlock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }

            pthread_exit(NULL);
        }

        /* produce till task queue is full or producer is done */
        while (task_queue_get_size() < args->buffer_size && thread_pool.producer_done == 0
                && thread_pool.terminate == 0)
        {
            /* add task to task queue */           
            /* recursively search source directory and add tasks to task queue */
            block_all_signals();
            /* if destination directory does not exist, create it */
            if (mkdir(args->dest_dir, 0777) == -1)
            {
                if (errno != EEXIST)
                {
                    error_print("PRODUCER: mkdir");
                    unblock_all_signals();
                    return NULL;
                }
                else
                {
                    /* if destination directory exists, append source to it */
                    /* destination_dir/source_dir */
                    snprintf(buffer, MAX_PATH_LEN, "%s/%s", args->dest_dir, args->source_dir);
                    snprintf(args->dest_dir, MAX_PATH_LEN, "%s", buffer);

                    /* create destination directory */
                    if (mkdir(args->dest_dir, 0777) == -1)
                    {
                        if (errno != EEXIST)
                        {
                            error_print("PRODUCER: mkdir");
                            unblock_all_signals();
                            return NULL;
                        }
                    }
                    else
                    {
                        pcp_stats.num_dir_files++;
                    }
                }
            }
            else
            {
                pcp_stats.num_dir_files++;
            }


            produce_files(args->source_dir, args->dest_dir, &args->buffer_size);
            thread_pool.producer_done = 1;
            unblock_all_signals();
        }

        thread_pool.producer_done = 1;

        /* unlock mutex */
        if (pthread_mutex_unlock(&thread_pool.mutex) != 0)
        {
            error_print("pthread_mutex_unlock");
            return NULL;
        }

        /* if task queue is full or producer is done signal consumer threads */
        if (task_queue_get_size() == args->buffer_size || thread_pool.producer_done == 1)
        {
            if (pthread_cond_broadcast(&thread_pool.full) != 0)
            {
                error_print("pthread_cond_broadcast");
                return NULL;
            }
        }

        /* check if producer is done */
        if (thread_pool.producer_done == 1)
        {
            /* lock stdout mutex */
            if (pthread_mutex_lock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_lock");
                return NULL;
            }

            fprintf(stderr, "\nPRODUCER: Producing is done, terminating\n");

            /* unlock stdout mutex */
            if (pthread_mutex_unlock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }

            pthread_exit(NULL);
        }
    }
}

void *consumer_worker (void *arg)
{
    consumer_args_t *args = (consumer_args_t *) arg;
    while (1)
    {
        /* lock mutex and wait for signal */
        if (pthread_mutex_lock(&thread_pool.mutex) != 0)
        {
            error_print("pthread_mutex_lock");
            return NULL;
        }

        while (task_queue_get_size() == 0 && thread_pool.terminate == 0 && thread_pool.producer_done == 0)
        {
            /* signal producer thread */
            if (pthread_cond_signal(&thread_pool.empty) != 0)
            {
                error_print("pthread_cond_signal");
                return NULL;
            }

            if ((thread_pool.producer_done == 0) &&
                pthread_cond_wait(&thread_pool.full, &thread_pool.mutex) != 0)
            {
                error_print("pthread_cond_wait");
                return NULL;
            }
        }

        /* check if thread pool is terminating */
        if (thread_pool.terminate == 1 && task_queue_get_size() == 0)
        {
            thread_pool.num_consumers_working--;

            /* signal producer thread */
            if (pthread_cond_signal(&thread_pool.empty) != 0)
            {
                error_print("pthread_cond_signal");
            }

            if (pthread_mutex_unlock(&thread_pool.mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }
            pthread_exit(NULL);
        }

        /* check if producer is done or terminate is set */
        if ( (thread_pool.producer_done == 1 && task_queue_get_size() == 0) ||
            (thread_pool.terminate == 1 && task_queue_get_size() == 0))
        {
            thread_pool.num_consumers_working--;

            /* unlock mutex */
            if (pthread_mutex_unlock(&thread_pool.mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }
            pthread_exit(NULL);
        }

        /* increase number of working consumers */
        thread_pool.num_consumers_working++;

        /* get task from task queue */
        task_t task;
        if (poll_task(&task) == -1)
        {
            error_print_custom("CONSUMER: poll_task");
            return NULL;
        }

        /* unlock mutex */
        if (pthread_mutex_unlock(&thread_pool.mutex) != 0)
        {
            error_print("pthread_mutex_unlock");
            return NULL;
        }

        /* if task queue is empty signal producer thread */
        if (task_queue_get_size() == 0 && thread_pool.producer_done == 0)
        {
            if (pthread_cond_signal(&thread_pool.empty) != 0)
            {
                error_print("pthread_cond_signal");
                return NULL;
            }
        }

        /* process task */
        if (process_task(args->dest_dir, &task) == -1)
        {
            error_print_custom("CONSUMER: File could not be copied");
            return NULL;
        }

        if (signal_occurred)
        {
            /* lock stdout mutex */
            if (pthread_mutex_lock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_lock");
                return NULL;
            }
            fprintf(stderr, "\nCONSUMER: Signal occurred\n");

            /* unlock stdout mutex */
            if (pthread_mutex_unlock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return NULL;
            }
            
            if (signal_occurred == SIGINT || signal_occurred == SIGQUIT)
                thread_pool.terminate = 1;
            signal_occurred = 0;
        }        
    }
}

int produce_files(const char *source_dir, const char *dest_dir, const size_t *buffer_size)
{
    struct dirent *entry;
    DIR *dir;
    char full_path[MAX_PATH_LEN], full_path2[MAX_PATH_LEN];
    struct stat st;
    task_t task;
    size_t i;

    dir = opendir(source_dir);

    if (dir == NULL)
    {
        error_print("PRODUCER: opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (thread_pool.terminate == 1)
        {
            closedir(dir);
            return 0;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(full_path, MAX_PATH_LEN, "%s/%s", source_dir, entry->d_name);

        if (stat(full_path, &st) == -1)
        {
            error_print("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            /* create directory in destination directory, if it doesn't exist */
            
            /* copy to full_path2, but ignore source_dir characters till the first / */
            snprintf(full_path2, MAX_PATH_LEN, "%s/", dest_dir);
            for (i = 0; i < strlen(full_path); ++i)
            {
                if (full_path[i] == '/')
                    break;
            }
            strncat(full_path2, full_path + i + 1, MAX_PATH_LEN - strlen(full_path2) - 1);

            if (mkdir(full_path2, 0777) == -1)
            {
                if (errno != EEXIST)
                {
                    error_print("PRODUCER: cannot create directory in destination directory");
                    continue;
                }
            }
            else
            {
                pcp_stats.num_dir_files++;
            }

            /* recursively search directory */
            produce_files(full_path, dest_dir, buffer_size);
        }
        else if (S_ISREG(st.st_mode))
        {
            /* add file as a task */
            task.read_fd = -1;
            task.write_fd = -1;
            task.file_type = FILE_TYPE_REGULAR;
            snprintf(task.file_name, MAX_PATH_LEN, "%s", full_path);
            /* open file */
            if ((task.read_fd = open(task.file_name, O_RDONLY)) == -1)
            {
                error_print("PRODUCER: open source file");
                continue;
            }
            else
            {
                pcp_stats.num_reg_files++;
                pcp_stats.num_open_file_desc++;
            }

            /* open file in destination directory */
            snprintf(full_path2, MAX_PATH_LEN, "%s/", dest_dir);
            for (i = 0; i < strlen(full_path); ++i)
            {
                if (full_path[i] == '/')
                    break;
            }
            strncat(full_path2, full_path + i + 1, MAX_PATH_LEN - strlen(full_path2) - 1);
            snprintf(task.file_name, MAX_PATH_LEN, "%s", full_path2);

            task.write_fd = open(full_path2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (task.write_fd == -1)
            {
                error_print("PRODUCER: open destination file");
                continue;
            }
            else
            {
                pcp_stats.num_open_file_desc++;
            }

            /* add task to task queue */
            offer_task(&task);
            
            /* fprintf(stderr, "PRODUCER: number of open file descriptors: %ld\n", pcp_stats.num_open_file_desc); */

            /* check buffer size and wait for signal if it's full */
            while ((task_queue_get_size() >= *buffer_size && thread_pool.terminate == 0) ||
                    (pcp_stats.num_open_file_desc >= open_fd_limit - 1))
            {
                /* signal consumer threads */
                if (pthread_cond_broadcast(&thread_pool.full) != 0)
                {
                    error_print("pthread_cond_broadcast");
                    return -1;
                }

                if (pthread_cond_wait(&thread_pool.empty, &thread_pool.mutex) != 0)
                {
                    error_print("pthread_cond_wait");
                    return -1;
                }                
            }
        }

        /* fifo */
        else if (S_ISFIFO(st.st_mode))
        {
            /* add fifo as a task */
            task.read_fd = -1;
            task.write_fd = -1;
            task.file_type = FILE_TYPE_FIFO;
            snprintf(task.file_name, MAX_PATH_LEN, "%s", full_path);

            /* open fifo in destination directory */
            snprintf(full_path2, MAX_PATH_LEN, "%s/", dest_dir);
            for (i = 0; i < strlen(full_path); ++i)
            {
                if (full_path[i] == '/')
                    break;
            }
            strncat(full_path2, full_path + i + 1, MAX_PATH_LEN - strlen(full_path2) - 1);

            /* create fifo */
            if (mkfifo(full_path2, 0666) == -1)
            {
                if (errno != EEXIST)
                {
                    error_print("PRODUCER: mkfifo");
                    continue;
                }
            }
            else
            {
                pcp_stats.num_fifo_files++;
            }

            /* check buffer size and wait for signal if it's full */
            while (task_queue_get_size() >= *buffer_size && thread_pool.terminate == 0)
            {
                /* signal consumer threads */
                if (pthread_cond_broadcast(&thread_pool.full) != 0)
                {
                    error_print("pthread_cond_broadcast");
                    return -1;
                }
                if (pthread_cond_wait(&thread_pool.empty, &thread_pool.mutex) != 0)
                {
                    error_print("pthread_cond_wait");
                    return -1;
                }                
            }
        }

        else
        {
            error_print_custom("Unknown file type");
        }

        /* check if signal occurred */
        if (signal_occurred)
        {
            /* lock stdout mutex */
            if (pthread_mutex_lock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_lock");
                return -1;
            }
            fprintf(stderr, "\nPRODUCER: Signal occurred\n");

            /* unlock stdout mutex */
            if (pthread_mutex_unlock(&thread_pool.stdout_mutex) != 0)
            {
                error_print("pthread_mutex_unlock");
                return -1;
            }
            
            if (signal_occurred == SIGINT || signal_occurred == SIGQUIT)
                thread_pool.terminate = 1;
            signal_occurred = 0;
        }
    }

    closedir(dir);
    return 0;
}

int process_task (const char *dest_dir, task_t *task)
{
    size_t total_bytes;
    total_bytes = 0;

    if (dest_dir == NULL)
        error_print_custom("CONSUMER: invalid destination directory");

    block_all_signals();
    /* copy file */
    if (task->file_type == FILE_TYPE_REGULAR)
    {
        if (copy_file(task->read_fd, task->write_fd, &total_bytes) == -1)
        {
            error_print_custom("CONSUMER: copy_file");
            unblock_all_signals();
            return -1;
        }
    }
    
    /* print information message */
    /* lock stdout mutex */
    if (pthread_mutex_lock(&thread_pool.stdout_mutex) != 0)
    {
        error_print("pthread_mutex_lock");
        unblock_all_signals();
        return -1;
    }

    pcp_stats.total_bytes += total_bytes;
    pcp_stats.num_open_file_desc -= 2;
    fprintf(stderr, "\nCONSUMER THREAD ID: %ld\nFile successfully copied: %s\n", pthread_self(), task->file_name);

    /* unlock stdout mutex */
    if (pthread_mutex_unlock(&thread_pool.stdout_mutex) != 0)
    {
        error_print("pthread_mutex_unlock");
        unblock_all_signals();
        return -1;
    }

    unblock_all_signals();

    return 0;
}

int copy_file (const int source_fd, const int dest_fd, size_t *total_bytes)
{
    ssize_t file_size;
    ssize_t buffer_size;
    char *buffer;
    ssize_t bytes_read;

    buffer_size = 4096;

    /* get file size */
    if ((file_size = lseek(source_fd, 0, SEEK_END)) == -1)
    {
        error_print("CONSUMER: lseek");
        return -1;
    }

    lseek(source_fd, 0, SEEK_SET);

    /* allocate buffer */
    if ((buffer = malloc(sizeof(char) * buffer_size)) == NULL)
    {
        error_print("CONSUMER: malloc");
        return -1;
    }

    /* copy file buffer_size bytes at a time */
    while (file_size > 0)
    {
        if (file_size < buffer_size)
            buffer_size = file_size;

        if ((bytes_read = read(source_fd, buffer, buffer_size)) == -1)
        {
            error_print("CONSUMER: read");
            free(buffer);
            return -1;
        }

        if (write(dest_fd, buffer, bytes_read) == -1)
        {
            error_print("CONSUMER: write");
            free(buffer);
            return -1;
        }
        *total_bytes += bytes_read;
        file_size -= bytes_read;
    }

    /* free buffer, close files */
    free(buffer);
    close(source_fd);
    close(dest_fd);
    return 0;
}