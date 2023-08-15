#include "include/thread_pool.h"
#include "include/mycommon.h"

sig_atomic_t signal_occurred = 0;

void signal_handler (int signal)
{
    if (signal == SIGINT)
        signal_occurred = 1;
}

int main (int argc, char *argv[])
{
    int port;
    char *directory;
    int server_socket;
    struct sigaction sa;
    socklen_t client_address_size;
    struct sockaddr_in client_address;
    char buffer[MAX_BUF_LEN];
    size_t thread_pool_size;
    task_t task;
    int client_socket;
    struct sockaddr_in server_address;

    /* input format is ./bibakBOXServer <directory> <thread_pool_size> <port> */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <directory> <thread_pool_size> <port>\n", argv[0]);
        return -1;
    }

    /* register signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);    
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    /* assign directory */
    directory = argv[1];

    /* assign thread pool size */
    thread_pool_size = string_to_uint(argv[2], strlen(argv[2]));

    if (thread_pool_size <= 0)
    {
        fprintf(stderr, "thread_pool_size must be positive\n");
        return -1;
    }

    /* assign port */
    port = string_to_int(argv[3], strlen(argv[3]));

    /* print inputs */
    printf("directory: %s\n", directory);
    printf("thread_pool_size: %zu\n", thread_pool_size);
    printf("port: %d\n", port);

    block_all_signals_thread();

    /* set printf buffer to NULL */
    setbuf(stdout, NULL);

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
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_print("socket");
        unblock_all_signals_thread();
        return -1;
    }

    /* bind socket */
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    /* localhost */
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1)
    {
        error_print("bind");
        unblock_all_signals_thread();
        return -1;
    }

    /* listen socket */
    if (listen(server_socket, 5) == -1)
    {
        error_print("listen");
        unblock_all_signals_thread();
        return -1;
    }

    unblock_all_signals_thread();

    /* create thread pool */
    if (thread_pool_init(thread_pool_size, directory) == -1)
    {
        error_print("thread_pool_init");
        return -1;
    }

    /* accept connections */

    while (1)
    {
        if (signal_occurred)
        {
            fprintf(stderr, "SERVER: Signal occurred, terminating...\n");
            thread_pool_destroy();
            break;
        }

        client_address_size = sizeof(client_address);
        if ((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_size)) == -1)
        {
            if (errno == EINTR)
                continue;
            error_print("accept");
            close(server_socket);
            return -1;
        }
        
        /* send client to message */
        memset(buffer, 0, MAX_BUF_LEN);
        if (thread_pool_is_full() == 0)
            snprintf(buffer, MAX_BUF_LEN, "Connection SUCCESSFUL");
        else
        {
            fprintf(stdout, "Client connection is refused, server is full\n");
            snprintf(buffer, MAX_BUF_LEN, "Connection FAILED, Server is FULL");

            if (write_str_msg(client_socket, buffer) == -1)
            {
                if (errno != EINTR)
                    error_print("write");
                close(client_socket);
                continue;
            }
            
            close(client_socket);
            continue;
        }
        
        if (write_str_msg(client_socket, buffer) == -1)
        {
            if (errno != EINTR)
                error_print("write");
            close(client_socket);
            continue;
        }

        /* receive client message */
        memset(buffer, 0, MAX_BUF_LEN);
        if (read_str_msg(client_socket, buffer) == -1)
        {
            if (errno != EINTR)
                error_print("read");
            close(client_socket);
            continue;
        }

        fprintf(stdout, "CLIENT: %s\n", buffer);

        /* set task */
        memset(&task, 0, sizeof(task_t));
        task.client_socket = client_socket;

        block_all_signals_thread();
        /* add task to thread pool */
        if (thread_pool_add_task(&task) == -1)
        {
            error_print_custom("thread_pool_add_task");
            close(client_socket);
            unblock_all_signals_thread();
            continue;
        }
        unblock_all_signals_thread();
    }
    close(server_socket);

    fprintf(stdout, "SERVER: Terminating...\n");
    return 0;
}