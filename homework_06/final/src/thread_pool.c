#include "../include/thread_pool.h"

thread_pool_t *thread_pool = NULL;

int thread_pool_init (const size_t thread_pool_size, char *dir_name)
{
    size_t i;

    block_all_signals_thread();
    /* allocate memory for thread pool */
    if ((thread_pool = malloc(sizeof(thread_pool_t))) == NULL)
    {
        error_print("malloc");
        return -1;
    }

    /* initialize thread pool */
    thread_pool->num_threads = thread_pool_size;
    thread_pool->active_threads = 0;
    thread_pool->shutdown = 0;
    thread_pool->dir_name = dir_name;

    /* create threads */
    if ((thread_pool->threads = malloc(sizeof(pthread_t) * thread_pool_size)) == NULL)
    {
        error_print("malloc");
        unblock_all_signals_thread();
        return -1;
    }

    /* initialize mutex */
    if (pthread_mutex_init(&thread_pool->mutex, NULL) != 0)
    {
        error_print("pthread_mutex_init");
        unblock_all_signals_thread();
        return -1;
    }
    
    /* initialize rwlock mutex */
    if (pthread_rwlock_init(&thread_pool->rwlock, NULL) != 0)
    {
        error_print("pthread_rwlock_init");
        unblock_all_signals_thread();
        return -1;
    }

    /* initialize condition variable */
    if (pthread_cond_init(&thread_pool->cond, NULL) != 0)
    {
        error_print("pthread_cond_init");
        unblock_all_signals_thread();
        return -1;
    }

    /* initalize task queue */
    if (task_queue_init() == -1)
    {
        error_print_custom("task_queue_init");
        unblock_all_signals_thread();
        return -1;
    }

    unblock_all_signals_thread();

    block_all_signals_thread();
    /* create threads */
    for (i = 0; i < thread_pool_size; ++i)
    {
        if (pthread_create(&thread_pool->threads[i], NULL, worker_thread, NULL) != 0)
        {
            error_print("pthread_create");        
            return -1;
        }
    }
    unblock_all_signals_thread();

    fprintf(stderr, "SV_THREAD: Thread pool created\n");
    return 0;
}

int thread_pool_destroy ()
{
    size_t i;
    thread_pool->shutdown = 1;

    block_all_signals_thread();
    /* broadcast to all threads */
    if (pthread_cond_broadcast(&thread_pool->cond) != 0)
    {
        error_print("pthread_cond_broadcast");
        unblock_all_signals_thread();
        return -1;
    }

    /* wait for all threads to terminate */
    for (i = 0; i < thread_pool->num_threads; ++i)
    {
        if (pthread_join(thread_pool->threads[i], NULL) != 0)
        {
            error_print("pthread_join");
            unblock_all_signals_thread();
            return -1;
        }
    }

    /* destroy mutex */
    if (pthread_mutex_destroy(&thread_pool->mutex) != 0)
    {
        error_print("pthread_mutex_destroy");        
        unblock_all_signals_thread();
        return -1;
    }

    /* destroy rwlock mutex */
    if (pthread_rwlock_destroy(&thread_pool->rwlock) != 0)
    {
        error_print("pthread_rwlock_destroy");        
        unblock_all_signals_thread();
        return -1;
    }

    /* destroy condition variable */
    if (pthread_cond_destroy(&thread_pool->cond) != 0)
    {
        error_print("pthread_cond_destroy");
        unblock_all_signals_thread();
        return -1;
    }

    /* free thread pool */
    free(thread_pool->threads);

    /* free thread pool */
    free(thread_pool);

    unblock_all_signals_thread();

    return 0;
}

int thread_pool_add_task (const task_t *task)
{
    /* lock mutex */
    pthread_mutex_lock(&thread_pool->mutex);

    /* add task to queue */
    if (offer_task(task) == -1)
    {
        error_print_custom("offer_task");
        pthread_mutex_unlock(&thread_pool->mutex);
        return -1;
    }

    /* broadcast to all threads */
    if (pthread_cond_broadcast(&thread_pool->cond) != 0)
    {
        error_print("pthread_cond_broadcast");
        pthread_mutex_unlock(&thread_pool->mutex);
        return -1;
    }

    /* unlock mutex */
    pthread_mutex_unlock(&thread_pool->mutex);

    return 0;
}

size_t thread_pool_get_num_threads ()
{
    return thread_pool->num_threads;
}

/* if the thread pool is full, return 1, else return 0 */
int thread_pool_is_full ()
{
    /* lock mutex */
    pthread_mutex_lock(&thread_pool->mutex);

    if (thread_pool->active_threads == thread_pool->num_threads)
    {
        /* unlock mutex */
        pthread_mutex_unlock(&thread_pool->mutex);
        return 1;
    }

    /* unlock mutex */
    pthread_mutex_unlock(&thread_pool->mutex);

    return 0;
}

void *worker_thread (void *args)
{
    task_t task;

    /* suppress unused parameter warning */
    (void) args;

    /* lock mutex */
    lock_mutex1();

    while (1)
    {
        /* check for signal */
        if (signal_occurred)
        {
            thread_pool->shutdown = 1;

            /* unlock mutex */
            unlock_mutex1();

            block_all_signals_thread();

            fprintf(stderr, "SV_THREAD: Signal occurred, terminating...\n");
            pthread_exit(NULL);
        }

        /* wait for a task to be added */
        while (task_queue_get_size() == 0 && thread_pool->shutdown == 0)
        {
            if (pthread_cond_wait(&thread_pool->cond, &thread_pool->mutex) != 0)
            {
                if (errno == EINTR)
                    continue;
                
                fprintf(stderr, "SV_THREAD: pthread_cond_wait\n");
                return NULL;
            }
        }

        /* check if thread pool is shutting down */
        if (thread_pool->shutdown == 1)
        {
            /* unlock mutex */
            unlock_mutex1();

            block_all_signals_thread();
            pthread_exit(NULL);
        }

        /* increment active threads */
        thread_pool->active_threads++;

        /* get task */
        if (poll_task(&task) == -1)
        {
            error_print_custom("poll_task");
            unlock_mutex1();
            return NULL;
        }

        fprintf(stderr, "Active Clients: %ld\n", thread_pool->active_threads);

        /* unlock mutex */
        unlock_mutex1();

        /* execute task */
        client_worker(&task);

        /* lock mutex */
        lock_mutex1();

        /* decrement active threads */
        thread_pool->active_threads--;

        fprintf(stderr, "Active Clients: %ld\n", thread_pool->active_threads);
    }

    fprintf(stderr, "SV_THREAD: Thread %lu terminated2\n", pthread_self());

    return NULL;
}

int client_worker (task_t *task)
{
    int client_socket;
    char buffer[MAX_BUF_LEN];
    message_t message;
    comp_result_t comp_result_cur;
    int file_fd;
    int ret;
    int change_flag = 0;
    file_info_list_t *head_cur = NULL;
    file_info_list_t *tail_cur = NULL;
    file_info_list_t *head_prev = NULL;
    file_info_list_t *tail_prev = NULL;
    comp_result_list_t *comp_list = NULL;
    comp_result_list_t *comp_list_temp = NULL;
    int delete_flag = 0;
    int accept_flag = 0;

    client_socket = task->client_socket;

    fprintf(stderr, "SV_THREAD: Client connected\n");

    while (1)
    {
        if (signal_occurred)
        {
            fprintf(stderr, "SV_THREAD: Signal occurred, terminating...\n");
            /* send exit message to client */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_EXIT;

            if (send_message(client_socket, &message, NULL) == -1)
            {
                error_print_custom("send_message");
                close(client_socket);
                break;
            }
            
            close(client_socket);
            break;
        }

        /* do checking */

        block_all_signals_thread();

        /* acquire read lock */
        pthread_rwlock_rdlock(&thread_pool->rwlock);

        if (checking_func (&head_prev, &tail_prev, &head_cur, &tail_cur, &comp_list, &change_flag, thread_pool->dir_name) == -1)
        {
            pthread_rwlock_unlock(&thread_pool->rwlock);

            error_print_custom("checking_func");
            
            /* send exit message to client */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_EXIT;

            if (send_message(client_socket, &message, NULL) == -1)
            {
                error_print_custom("send_message");
                close(client_socket);
                break;
            }

            close(client_socket);
            break;
        }

        pthread_rwlock_unlock(&thread_pool->rwlock);
        unblock_all_signals_thread();

        /* check if thread pool is shutting down */
        if (thread_pool->shutdown == 1)
        {
            /* send exit message to client */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_EXIT;

            block_all_signals_thread();
            if (send_message(client_socket, &message, NULL) == -1)
            {
                error_print_custom("send_message");
                close(client_socket);
                break;
            }
            close(client_socket);
            destroy_file_info_list_prev(&head_prev, &tail_prev);
            destroy_file_info_list_current(&head_cur, &tail_cur);
            free_comp_result_list(&comp_list);
            unblock_all_signals_thread();
            return 0;
        }

        /* server to client */

        /* if there is a change in the directory */
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
                    snprintf(buffer, MAX_BUF_LEN, "%s/%s", thread_pool->dir_name, comp_list_temp->comp_result.file_info.path);
                    
                    if (comp_list_temp->comp_result.file_info.file_type == REGULAR_FILE_TYPE)
                    {
                        block_all_signals_thread();
                        /* open file */
                        if ((file_fd = open(buffer, O_RDONLY)) == -1)
                        {
                            error_print("open");
                            comp_list_temp = comp_list_temp->next;
                            unblock_all_signals_thread();
                            continue;
                        }
                        unblock_all_signals_thread();
                    }
                }

                block_all_signals_thread();
                if (send_message(client_socket, &message, &comp_list_temp->comp_result) == -1)
                {
                    fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                    close(client_socket);
                    destroy_file_info_list_prev(&head_prev, &tail_prev);
                    destroy_file_info_list_current(&head_cur, &tail_cur);
                    free_comp_result_list(&comp_list);
                    return 0;
                }
                unblock_all_signals_thread();
                
                block_all_signals_thread();
                /* receive feedback from client */
                memset(&message, 0, sizeof(message_t));
                if (receive_message(client_socket, &message, &comp_result_cur) == -1)
                {
                    fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                    close(client_socket);
                    destroy_file_info_list_prev(&head_prev, &tail_prev);
                    destroy_file_info_list_current(&head_cur, &tail_cur);
                    free_comp_result_list(&comp_list);
                    return 0;
                }
                unblock_all_signals_thread();

                if (message.command == CM_DONE)
                {
                    /* it means given file is directory or fifo, so no need to send file */
                    comp_list_temp = comp_list_temp->next;
                    continue;
                }

                /* if it is rejected, close file and continue */
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
                    block_all_signals_thread();

                    /* acquire read lock */
                    pthread_rwlock_rdlock(&thread_pool->rwlock);

                    /* send file to client */
                    if (send_file(client_socket, file_fd) == -1)
                    {
                        fprintf(stderr, "SV_THREAD: Cannot send file %s to client, terminating...\n", comp_list_temp->comp_result.file_info.path);
                        close(file_fd);
                        close(client_socket);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);

                        /* release read lock */
                        pthread_rwlock_unlock(&thread_pool->rwlock);
                        return -1;
                    }
                    /* close file */
                    close(file_fd);
                    
                    /* release read lock */
                    pthread_rwlock_unlock(&thread_pool->rwlock);
                    unblock_all_signals_thread();

                    block_all_signals_thread();
                    /* receive done message */
                    memset(&message, 0, sizeof(message_t));
                    if (receive_message(client_socket, &message, NULL) == -1)
                    {
                        fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                        close(client_socket);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }
                    unblock_all_signals_thread();

                    if (message.command != CM_DONE)
                    {
                        fprintf(stderr, "SV_THREAD: Invalid message received from client\n");
                        comp_list_temp = comp_list_temp->next;
                        continue;
                    }
                }

                if (message.command == CM_ACCEPT && delete_flag == 1)
                {
                    block_all_signals_thread();
                    /* receive done message */
                    memset(&message, 0, sizeof(message_t));
                    if (receive_message(client_socket, &message, NULL) == -1)
                    {
                        fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                        close(client_socket);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }
                    unblock_all_signals_thread();
                }

                comp_list_temp = comp_list_temp->next;
            }

            /* send done message */
            memset(&message, 0, sizeof(message_t));
            message.command = CM_DONE;

            block_all_signals_thread();
            if (send_message(client_socket, &message, NULL) == -1)
            {
                fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                close(client_socket);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
            unblock_all_signals_thread();
        }
        else
        {
            memset(&message, 0, sizeof(message_t));
            message.command = CM_SAME;

            block_all_signals_thread();
            if (send_message(client_socket, &message, NULL) == -1)
            {
                fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                close(client_socket);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                
                return 0;
            }
            unblock_all_signals_thread();
        }

        /* client to server */

        /* receive files till CM_DONE, CM_EXIT or CM_SAME */
        while (1)
        {
            memset(&message, 0, sizeof(message_t));
            memset(&comp_result_cur, 0, sizeof(comp_result_t));

            block_all_signals_thread();
            if (receive_message(client_socket, &message, &comp_result_cur) == -1)
            {
                fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");

                close(client_socket);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
            else if (message.command == CM_DONE)
                break;
            else if (message.command == CM_EXIT)
            {
                fprintf(stderr, "SV_THREAD: Termination message received from client\n");
                close(client_socket);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return 0;
            }
            else if (message.command == CM_SAME)
            {
                break;
            }
            else if (message.command == CM_CHANGE)
            {
                accept_flag = 0;
                unblock_all_signals_thread();


                block_all_signals_thread();
                pthread_rwlock_rdlock(&thread_pool->rwlock);

                /* check file stats */
                if (is_current_old(&comp_result_cur, thread_pool->dir_name) == 1)
                    accept_flag = 1;

                pthread_rwlock_unlock(&thread_pool->rwlock);
                unblock_all_signals_thread();

                if (accept_flag == 1 && comp_result_cur.file_info.file_type == DIRECTORY_TYPE)
                {
                    /* deletion */
                    if (comp_result_cur.comp_result == FILE_DELETED)
                    {
                        block_all_signals_thread();

                        /* acquire write lock */
                        pthread_rwlock_wrlock(&thread_pool->rwlock);

                        /* delete all files inside it */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", thread_pool->dir_name, comp_result_cur.file_info.path);
                        if (delete_all_files_and_dir(buffer, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                        {
                            accept_flag = 0;

                            /* release write lock */
                            pthread_rwlock_unlock(&thread_pool->rwlock);
                        }
                        else
                        {
                            /* release write lock */
                            pthread_rwlock_unlock(&thread_pool->rwlock);

                            /* send done message */
                            memset(&message, 0, sizeof(message_t));
                            message.command = CM_DONE;

                            if (send_message(client_socket, &message, NULL) == -1)
                            {
                                fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                                close(client_socket);
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
                    else if (comp_result_cur.comp_result == FILE_ADDED || comp_result_cur.comp_result == FILE_MODIFIED)
                    {
                        
                        /* acquire write lock */
                        pthread_rwlock_wrlock(&thread_pool->rwlock);

                        block_all_signals_thread();
                        /* create directories before it */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", thread_pool->dir_name, comp_result_cur.file_info.path);
                        if (create_dirs(buffer, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                        {
                            accept_flag = 0;

                            /* release write lock */
                            pthread_rwlock_unlock(&thread_pool->rwlock);
                        }
                        else
                        {
                            /* create directory */
                            if (mkdir(buffer, 0777) == -1)
                            {
                                accept_flag = 0;

                                /* release write lock */
                                pthread_rwlock_unlock(&thread_pool->rwlock);
                            }
                            else
                            {

                                /* release write lock */
                                pthread_rwlock_unlock(&thread_pool->rwlock);

                                if (update_file_info_list(&comp_result_cur, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                                {
                                    error_print_custom("update_file_info_list");
                                }

                                /* send done message */
                                memset(&message, 0, sizeof(message_t));
                                message.command = CM_DONE;

                                if (send_message(client_socket, &message, NULL) == -1)
                                {
                                    fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                                    close(client_socket);
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
                else if (accept_flag == 1 && comp_result_cur.file_info.file_type == FIFO_FILE_TYPE)
                {
                    /* deletion */
                    if (comp_result_cur.comp_result == FILE_DELETED)
                    {
                        block_all_signals_thread();

                        /* acquire write lock */
                        pthread_rwlock_wrlock(&thread_pool->rwlock);

                        /* delete fifo */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", thread_pool->dir_name, comp_result_cur.file_info.path);
                        if (unlink(buffer) == -1)
                        {
                            fprintf(stderr, "SV_THREAD: Cannot delete fifo %s\n", buffer);
                            accept_flag = 0;

                            /* release write lock */
                            pthread_rwlock_unlock(&thread_pool->rwlock);
                        }
                        else
                        {
                            /* release write lock */
                            pthread_rwlock_unlock(&thread_pool->rwlock);

                            /* update file info list */
                            if (update_file_info_list(&comp_result_cur, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                            {
                                error_print_custom("update_file_info_list");
                            }

                            /* send done message */
                            memset(&message, 0, sizeof(message_t));
                            message.command = CM_DONE;

                            if (send_message(client_socket, &message, NULL) == -1)
                            {
                                fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                                close(client_socket);
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
                    else if (comp_result_cur.comp_result == FILE_ADDED || comp_result_cur.comp_result == FILE_MODIFIED)
                    {
                        block_all_signals_thread();
                        pthread_rwlock_wrlock(&thread_pool->rwlock);

                        /* create directories before it */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", thread_pool->dir_name, comp_result_cur.file_info.path);
                        if (create_dirs(buffer, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                        {
                            accept_flag = 0;

                            /* release write lock */
                            pthread_rwlock_unlock(&thread_pool->rwlock);
                        }
                        else
                        {
                            /* create fifo */
                            if (mkfifo(buffer, 0777) == -1)
                            {
                                accept_flag = 0;

                                /* release write lock */
                                pthread_rwlock_unlock(&thread_pool->rwlock);
                            }
                            else
                            {
                                /* release write lock */
                                pthread_rwlock_unlock(&thread_pool->rwlock);

                                /* acquire read lock */
                                pthread_rwlock_rdlock(&thread_pool->rwlock);

                                /* update file info list */
                                if (update_file_info_list(&comp_result_cur, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                                {
                                    error_print_custom("update_file_info_list");
                                }

                                /* release read lock */
                                pthread_rwlock_unlock(&thread_pool->rwlock);

                                /* send done message */
                                memset(&message, 0, sizeof(message_t));
                                message.command = CM_DONE;

                                if (send_message(client_socket, &message, NULL) == -1)
                                {
                                    fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                                    close(client_socket);
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
                    if (comp_result_cur.comp_result == FILE_DELETED)
                        delete_flag = 1;

                    block_all_signals_thread();

                    /* acquire write lock */
                    pthread_rwlock_wrlock(&thread_pool->rwlock);

                    /* open file and create directories if necessary, if it is not deleted */
                    if (delete_flag == 0 &&
                        (ret = open_file_and_create_dirs(&comp_result_cur, thread_pool->dir_name, &file_fd, &head_cur, &tail_cur)) == -1)
                    {
                        /* release write lock */
                        pthread_rwlock_unlock(&thread_pool->rwlock);

                        /* send reject message to client */
                        memset(&message, 0, sizeof(message_t));
                        message.command = CM_REJECT;

                        if (send_message(client_socket, &message, NULL) == -1)
                        {
                            fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                            close(client_socket);
                            destroy_file_info_list_prev(&head_prev, &tail_prev);
                            destroy_file_info_list_current(&head_cur, &tail_cur);
                            free_comp_result_list(&comp_list);
                            return 0;
                        }

                        unblock_all_signals_thread();
                        continue;
                    }
                    /* release write lock */
                    pthread_rwlock_unlock(&thread_pool->rwlock);

                    /* send accept message to client */
                    memset(&message, 0, sizeof(message_t));
                    message.command = CM_ACCEPT;

                    if (send_message(client_socket, &message, NULL) == -1)
                    {
                        fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                        
                        close(client_socket);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }

                    /* receive file from client */

                    /* acquire write lock */
                    pthread_rwlock_wrlock(&thread_pool->rwlock);

                    if (delete_flag == 0 &&
                        (receive_file_and_write(client_socket, file_fd) == -1))
                    {
                        fprintf(stderr, "SV_THREAD: Cannot receive file %s, terminating...\n", comp_result_cur.file_info.path);
                        close(file_fd);
                    
                        /* release write lock */
                        pthread_rwlock_unlock(&thread_pool->rwlock);
                        close(client_socket);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return -1;
                    }

                    else if (delete_flag == 1)
                    {
                        /* delete file */
                        snprintf(buffer, MAX_BUF_LEN, "%s/%s", thread_pool->dir_name, comp_result_cur.file_info.path);
                        unlink(buffer);
                    }

                    /* release write lock */
                    pthread_rwlock_unlock(&thread_pool->rwlock);

                    /* close file */
                    if (delete_flag == 0)
                        close(file_fd);
                    
                    /* send done message to server */
                    memset(&message, 0, sizeof(message_t));
                    message.command = CM_DONE;

                    if (send_message(client_socket, &message, NULL) == -1)
                    {
                        fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                        close(client_socket);
                        destroy_file_info_list_prev(&head_prev, &tail_prev);
                        destroy_file_info_list_current(&head_cur, &tail_cur);
                        free_comp_result_list(&comp_list);
                        return 0;
                    }

                    /* acquire read lock */
                    pthread_rwlock_rdlock(&thread_pool->rwlock);

                    /* update file info list */
                    if (update_file_info_list(&comp_result_cur, &head_cur, &tail_cur, thread_pool->dir_name) == -1)
                    {
                        error_print_custom("update_file_info_list");

                        /* release read lock */
                        pthread_rwlock_unlock(&thread_pool->rwlock);
                        unblock_all_signals_thread();
                        continue;
                    }

                    /* release read lock */
                    pthread_rwlock_unlock(&thread_pool->rwlock);
                    unblock_all_signals_thread();
                }
                else
                {
                    /* send reject message to client */
                    memset(&message, 0, sizeof(message_t));
                    message.command = CM_REJECT;

                    block_all_signals_thread();
                    if (send_message(client_socket, &message, NULL) == -1)
                    {
                        fprintf(stderr, "SV_THREAD: Client is disconnected, terminating...\n");
                        close(client_socket);
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
                fprintf(stderr, "SV_THREAD: Invalid message received from client, terminating...\n");
                
                close(client_socket);
                destroy_file_info_list_prev(&head_prev, &tail_prev);
                destroy_file_info_list_current(&head_cur, &tail_cur);
                free_comp_result_list(&comp_list);
                return -1;
            }

            unblock_all_signals_thread();
        }
    }

    destroy_file_info_list_prev(&head_prev, &tail_prev);
    destroy_file_info_list_current(&head_cur, &tail_cur);
    free_comp_result_list(&comp_list);

    return 0;
}

int block_all_signals_thread ()
{
    sigset_t set;

    sigfillset(&set);

    /* remove sigpipe from set */
    sigdelset(&set, SIGPIPE);

    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
    {
        error_print("pthread_sigmask");
        return -1;
    }

    return 0;
}

int unblock_all_signals_thread ()
{
    sigset_t set;

    sigfillset(&set);

    if (pthread_sigmask(SIG_UNBLOCK, &set, NULL) != 0)
    {
        error_print("pthread_sigmask");
        return -1;
    }

    return 0;
}

int lock_mutex1 ()
{
    block_all_signals_thread();
    if (pthread_mutex_lock(&thread_pool->mutex) != 0)
    {
        error_print("pthread_mutex_lock");
        unblock_all_signals_thread();
        return -1;
    }
    unblock_all_signals_thread();
    return 0;
}

int unlock_mutex1 ()
{
    block_all_signals_thread();
    if (pthread_mutex_unlock(&thread_pool->mutex) != 0)
    {
        error_print("pthread_mutex_unlock");
        unblock_all_signals_thread();
        return -1;
    }
    unblock_all_signals_thread();

    return 0;
}