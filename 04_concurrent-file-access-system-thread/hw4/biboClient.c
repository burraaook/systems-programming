#include "include/mycommon.h"
#include "include/biboClient.h"

sig_atomic_t signal_occured = 0;

void sig_handler (int signo)
{
    signal_occured = signo;
}

size_t addr_size = 0;

int main (int argc, char *argv[])
{
    char buffer[MAX_BUF_LEN];
    char sv_fifo_name[MAX_FILENAME_LEN];
    char cl_fifo_name[MAX_FILENAME_LEN];
    char file_name[MAX_FILENAME_LEN];
    char shm_name[MAX_FILENAME_LEN];
    request_t request;
    response_t response;
    pid_t server_pid;
    int cl_fd, shm_fd, sv_fd, temp_fd;
    struct sigaction sa;
    void *addr;

    request.client_pid = getpid();
    request.wait_flag = 0;

    /* set printf to be unbuffered */
    setbuf(stdout, NULL);

    /* set the signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_handler;
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    /* number of arguments is 2 */
    switch (argc)
    {
        case 3:
            /* connect or tryconnect */
            if (strcmp(argv[1], "Connect") == 0 || strcmp(argv[1], "TryConnect") == 0)
            {
                server_pid = string_to_uint(argv[2], strlen(argv[2]));
                
                snprintf(cl_fifo_name, MAX_FILENAME_LEN, "%s_%d", CL_FIFO_NAME, server_pid);
                /* open the fifo */
                if ((cl_fd = open(cl_fifo_name, O_WRONLY)) == -1)
                    error_exit("open");
                
                block_all_signals();

                fprintf(stderr, "Waiting for Que ..\n");

                /* create fifo for client connection requests */
                snprintf(sv_fifo_name, MAX_FILENAME_LEN, "%s_%d", SV_FIFO_NAME, request.client_pid);

                block_all_signals();
                if (mkfifo(sv_fifo_name, FIFO_PERM) == -1)
                {
                    if (errno == EEXIST)
                    {
                        if (unlink(sv_fifo_name) == -1)
                            error_exit("unlink");
                        
                        if (mkfifo(sv_fifo_name, FIFO_PERM) == -1)
                            error_exit("mkfifo");
                    }
                    else
                        error_exit("mkfifo");
                }
                unblock_all_signals();

                if (strcmp(argv[1], "Connect") == 0)
                    request.command = CMD_CONNECT;
                else
                    request.command = CMD_TRYCONNECT;

                block_all_signals();
                /* send the server pid */
                if (write(cl_fd, &request, sizeof(struct request_t)) == -1)
                    error_exit("write");
                unblock_all_signals();

                /* close the fifo */
                if (close(cl_fd) == -1)
                    error_exit("close 1");

                /* get the response */
                if ((sv_fd = open(sv_fifo_name, O_RDONLY)) == -1)
                    error_exit("open1");

                block_all_signals();
                if (read(sv_fd, &response, sizeof(struct request_t)) == -1)
                    error_exit("read fifo client");
                unblock_all_signals();

                /* close the fifo */
                if (close(sv_fd) == -1)
                    error_exit("close 2");

                if (response.flag == SV_FAILURE)
                {
                    if (request.command == CMD_CONNECT) 
                    {
                        printf("\nServer is full, waiting..\n");
                        request.wait_flag = 1;

                        /* open the fifo */
                        if ((sv_fd = open(sv_fifo_name, O_RDONLY)) == -1)
                        {
                            if (errno == EINTR)
                            {
                                fprintf(stderr, "> SIGINT received. Terminating..\n");
                                unlink(sv_fifo_name);
                                cleanup_and_terminate(request, cl_fifo_name);
                            }
                            else
                                error_exit("open");
                        }
                        
                        /* wait for response */
                        if (read(sv_fd, &response, sizeof(struct request_t)) == -1)
                            error_exit("read fifo client2");
                    }
                        
                    else
                    {
                        printf("Server is full, terminating..\n");
                        
                        /* remove the fifo */
                        if (unlink(sv_fifo_name) == -1)
                            error_exit("unlink");
                        
                        exit(EXIT_FAILURE);
                    }
                }

                request.wait_flag = 0;
                fprintf(stderr, "Connection established with server..\n");
            }
            else
                error_exit_custom("./biboClient Connect/TryConnect <server_pid>");
            break;
        default:
            error_exit_custom("./biboClient Connect/TryConnect <server_pid>");
    }

    block_all_signals();

    /* create the shared memory */
    snprintf(shm_name, MAX_FILENAME_LEN, "%s_%d", SHM_NAME, request.client_pid);
    shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (shm_fd == -1)
    {
        if (errno == EEXIST)
        {
            shm_fd = shm_open(shm_name, O_RDWR, 0666);
            if (shm_fd == -1)
                error_exit("shm_open");
        }
        else
            error_exit("shm_open");
    }
    
    /* truncate the shared memory */
    if (ftruncate(shm_fd, SHM_DEFAULT_SIZE) == -1)
        error_exit("ftruncate");

    /* map the shared memory */
    addr = mmap(NULL, SHM_DEFAULT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    addr_size = SHM_DEFAULT_SIZE;
    unblock_all_signals();

    /* get requests from stdin */
    while (1)
    {
        if (signal_occured > 0)
        {
            switch (signal_occured)
            {
                case SIGINT:
                    fprintf(stderr, "> SIGINT received. Terminating..\n");
                    unlink(sv_fifo_name);
                    shm_unlink(shm_name);
                    cleanup_and_terminate(request, cl_fifo_name);
                break;

                case SIGTERM: 
                    fprintf(stderr, "> SIGTERM received. Terminating..\n");
                    unlink(sv_fifo_name);
                    shm_unlink(shm_name);
                    cleanup_and_terminate(request, cl_fifo_name);
                break;

                case SIGQUIT:
                    fprintf(stderr, "> SIGQUIT received. Terminating..\n");
                    unlink(sv_fifo_name);
                    shm_unlink(shm_name);
                    cleanup_and_terminate(request, cl_fifo_name);
                break;                
            }

            signal_occured = 0;
        }
        fprintf(stdout, "\n>> Enter comment: ");

        /* reset buffer */
        memset(buffer, 0, MAX_BUF_LEN);

        /* read the request */
        if (fgets(buffer, MAX_BUF_LEN, stdin) == NULL)
        {
            if (errno == EINTR)
            {
                fprintf(stderr, "> SIGINT received. Terminating..\n");
                unlink(sv_fifo_name);
                shm_unlink(shm_name);
                cleanup_and_terminate(request, cl_fifo_name);
            }
            else
                error_print("fgets");
        }

        if (strlen(buffer) <= 1)
            continue;

        /* remove the newline character */
        buffer[strlen(buffer) - 1] = '\0';

        if (strlen(buffer) >= 4 && strncmp(buffer, "help", 4) == 0)
        {
            request.command = CMD_HELP;

            if (strlen(buffer) > 5)
            {
                /* get the command */
                write_next_word(buffer, request.file_name);

                /* remove the newline character */
                if (request.file_name[strlen(request.file_name) - 1] == '\n')
                    request.file_name[strlen(request.file_name) - 1] = '\0';

                if (strcmp(request.file_name, "readF") == 0)
                    request.command = CMD_HELP_READF;
                else if (strcmp(request.file_name, "writeT") == 0)
                    request.command = CMD_HELP_WRITET;
                else if (strcmp(request.file_name, "upload") == 0)
                    request.command = CMD_HELP_UPLOAD;
                else if (strcmp(request.file_name, "download") == 0)
                    request.command = CMD_HELP_DOWNLOAD;
                else if (strcmp(request.file_name, "list") == 0)
                    request.command = CMD_HELP_LIST;
                else if (strcmp(request.file_name, "quit") == 0)
                    request.command = CMD_HELP_QUIT;
                else if (strcmp(request.file_name, "killServer") == 0)
                    request.command = CMD_HELP_KILLSERVER;
                else
                {
                    fprintf(stderr, "Unknown command. help or help <command>\n");
                    continue;
                }
            }
            
            /* send help request to the server */
            if (send_request(cl_fifo_name, request) == -1)
                continue;

            /* wait for server response */
            if (get_response(sv_fifo_name, &response) == -1)
                continue;
            
            if (response.flag == SV_FAILURE)
                fprintf(stderr, "Unknown command\n");

            else 
            {
                /* print the response which is at the shared memory */
                
                block_all_signals();
                if (print_message_shm(&addr, shm_fd, response.file_size) == -1)
                    error_print_custom("print_message_shm");
                unblock_all_signals();
            }
        }

        else if (strlen(buffer) >= 5 && strncmp(buffer, "readF", 5) == 0)
        {
            if (strlen(buffer) <= 6)
            {
                error_print_custom("Filename is missing. readF <file> or readF <filename> <line #>");
                continue;
            }

            request.arg1 = 0;
            /* get first argument */
            if (write_next_2words(buffer, request.file_name, &request.arg1) == -1)
            {
                error_print_custom("Invalid command. readF <file> or readF <filename> <line #>");
                continue;
            }
            /* send readF request to the server */
            request.command = CMD_READF;
            
            if (send_request(cl_fifo_name, request) == -1)
                continue;

            /* get server response */
            if (get_response(sv_fifo_name, &response) == -1)
                continue;

            /* check if the file exists */
            if (response.flag == SV_FAILURE)
            {
                fprintf(stderr, "File does not exist\n");
                continue;
            }

            if (print_message_shm(&addr, shm_fd, response.file_size) == -1)
                error_print_custom("message cannot be printed");

        }

        else if (strlen(buffer) >= 6 && strncmp(buffer, "writeT", 6) == 0)
        {
            request.command = CMD_WRITET;
            request.arg1 = -1;
            if (strlen(buffer) <= 7)
            {
                error_print_custom("Filename is missing. writeT <filename> <line #> <string>");
                continue;
            }

            /* get the filename, line number */
            if (fill_arguments_w(buffer, request.file_name, &request.arg1, request.message) == -1)
            {
                error_print_custom("Invalid command. writeT <filename> <line #> <string> or writeT <filename> <string>");
                continue;
            }

            /* send writeT request to the server */
            if (send_request(cl_fifo_name, request) == -1)
                continue;

            /* get server response */
            if (get_response(sv_fifo_name, &response) == -1)
                continue;

            if (response.flag == SV_FAILURE)
                fprintf(stderr, "cannot write to file\n");
        }
        else if (strncmp(buffer, "upload", 6) == 0)
        {
            request.command = CMD_UPLOAD;

            if (strlen(buffer) <= 7)
            {
                error_print_custom("Filename is missing. upload <filename>");
                continue;
            }

            /* get the filename */
            write_next_word(buffer, file_name);
            
            block_all_signals();
            /* check if file exists */
            temp_fd = open(file_name, O_RDONLY);
            if (temp_fd == -1)
            {
                error_print_custom("File does not exist");
                unblock_all_signals();
                continue;
            }
            unblock_all_signals();
            /* get the file size */
            request.file_size = lseek(temp_fd, 0, SEEK_END);

            if (close(temp_fd) == -1)
                error_print("close");
            
            /* write file to the shared memory */

            fprintf(stderr, "> Sending upload request to the server..\n");
            fprintf(stderr, "> Writing file to shared memory..\n");

            block_all_signals();
            if (write_file_to_shm(file_name, &addr, shm_fd) == -1)
                error_print_custom("write_file_to_shm");
            unblock_all_signals();
            fprintf(stderr, "> File written to shared memory..\n");

            strcpy(request.file_name, file_name);

            /* send upload request to the server */
            if (send_request(cl_fifo_name, request) == -1)
            {
                error_print_custom("send_request");
                continue;
            }

            /* get the response */
            if (get_response(sv_fifo_name, &response) == -1)
            {
                error_print_custom("get_response");
                continue;
            }

            if (response.flag == SV_FAILURE)
                error_print_custom("Upload failed");
            else
                fprintf(stderr, "> Upload successful..\n");
        }
        else if (strncmp(buffer, "download", 8) == 0)
        {
            if (strlen(buffer) <= 9)
            {
                error_print_custom("Filename is missing. download <filename>");
                continue;
            }

            /* get the filename */
            write_next_word(buffer, file_name);

            /* send download request to the server */
            request.command = CMD_DOWNLOAD;
            strcpy(request.file_name, file_name);

            /* send download request to the server */
            if (send_request(cl_fifo_name, request) == -1)
                continue;

            /* get the response */
            if (get_response(sv_fifo_name, &response) == -1)
                continue;

            /* check if the file exists */
            if (response.flag == SV_FAILURE)
            {
                fprintf(stderr, "File does not exist\n");
                continue;
            }

            fprintf(stderr, "> Downloading file from shared memory..\n");

            block_all_signals();

            /* read the file from the shared memory */
            if (read_file_from_shm(file_name, &addr, shm_fd, response.file_size) == -1)
                error_print_custom("read_file_from_shm");
            
            fprintf(stderr, "> File downloaded from shared memory..\n");

            unblock_all_signals();

            fprintf(stderr, "> File downloaded..\n");
        }
        else if (strcmp(buffer, "list") == 0)
        {
            request.command = CMD_LIST;

            /* open the fifo */
            if ((cl_fd = open(cl_fifo_name, O_WRONLY)) == -1)
            {
                if (errno == EINTR)
                {
                    fprintf(stderr, "> SIGINT received. Terminating..\n");
                    unlink(sv_fifo_name);
                    shm_unlink(shm_name);
                    cleanup_and_terminate(request, cl_fifo_name);
                }
                else
                    error_print("open");
            }

            /* send the request */
            if (write(cl_fd, &request, sizeof(struct request_t)) == -1)
                error_print("write");

            /* close the fifo */
            if (close(cl_fd) == -1)
                error_print("close");

            /* wait for server response */
            /* open the fifo */

            if ((sv_fd = open(sv_fifo_name, O_RDONLY)) == -1)
            {
                if (errno == EINTR)
                {
                    fprintf(stderr, "> SIGINT received. Terminating..\n");
                    unlink(sv_fifo_name);
                    shm_unlink(shm_name);
                    cleanup_and_terminate(request, cl_fifo_name);
                }
                else
                    error_print("open");
            }

            /* read the response */
            if (read(sv_fd, &response, sizeof(struct request_t)) == -1)
                error_print("read fifo client");

            /* close the fifo */
            if (close(sv_fd) == -1)
                error_print("close");

            block_all_signals();
            /* print the response which is at the shared memory */
            if (response.flag != SV_FAILURE)
            {
                if (print_message_shm(&addr, shm_fd, response.file_size) == -1)
                    error_print_custom("print_message_shm");
            }
            unblock_all_signals();

        }
        else if (strcmp(buffer, "quit") == 0)
        {
            request.command = CMD_QUIT;
            unlink(sv_fifo_name);
            cl_fd = open(cl_fifo_name, O_WRONLY);
            write (cl_fd, &request, sizeof(struct request_t));
            break;
        }

        else if (strcmp(buffer, "killServer") == 0)
        {
            request.command = CMD_KILLSERVER;
            unlink(sv_fifo_name);
            cl_fd = open(cl_fifo_name, O_WRONLY);
            write (cl_fd, &request, sizeof(struct request_t));
            break;
        }

        else
        {
            fprintf(stderr, "Unknown command\n");
            continue;
        }
    }
    return 0;
}

void cleanup_and_terminate(request_t request, char *cl_fifo_name)
{
    int fd;

    request.command = CMD_QUIT;

    /* open the fifo */
    if ((fd = open(cl_fifo_name, O_WRONLY)) == -1)
        exit(EXIT_SUCCESS);

    /* send the request */
    if (write(fd, &request, sizeof(struct request_t)) == -1)
        exit(EXIT_SUCCESS);

    /* close the fifo */
    if (close(fd) == -1)
        error_exit("close");

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

int write_next_word (const char *buffer, char *word)
{
    int i, j;
    i = 0;
    j = 0;

    /* skip till the first space */
    while (buffer[i] != ' ' && buffer[i] != '\0')
        i++;
    
    if (buffer[i] == '\0')
        return -1;

    /* skip other spaces */
    while (buffer[i] == ' ' && buffer[i] != '\0')
        i++;

    /* copy the word */
    while (buffer[i] != ' ' && buffer[i] != '\0' && i < MAX_BUF_LEN && j < MAX_FILENAME_LEN)
    {
        word[j] = buffer[i];
        i++;
        j++;
    }
    word[j] = '\0';

    return 0;
}

int write_file_to_shm (const char *file_name, void **addr, int shm_fd)
{
    int fd;
    size_t file_size;
    char *shm_ptr;
    struct flock fl;

    /* open the file */
    if ((fd = open(file_name, O_RDONLY)) == -1)
    {
        error_print("open");
        return -1;
    }

    /* read lock the file */
    if (read_lock_file(fd, &fl) == -1)
    {
        error_print("read_lock_file");
        close(fd);
        return -1;
    }

    /* get the file size */
    file_size = lseek(fd, 0, SEEK_END);

    /* check if the file size is greater than the shared memory size */
    if (file_size > addr_size)
    {
        /* resize the shared memory */

        /* unmap the shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap");
            unlock_file(fd, &fl);
            close(fd);
            return -1;
        }
    
        /* truncate the file */
        if (ftruncate(shm_fd, file_size) == -1)
        {
            error_print("ftruncate");
            unlock_file(fd, &fl);
            close(fd);
            return -1;
        }

        /* map the file to the shared memory */
        *addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

        addr_size = file_size;
    }

    /* map the file to the shared memory */
    shm_ptr = (char *) *addr;

    /* set the file pointer to the beginning of the file */
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        error_print("lseek");
        unlock_file(fd, &fl);
        close(fd);
        return -1;
    }

    /* read the file */
    if (read(fd, shm_ptr, file_size) == -1)
    {
        error_print("read");
        unlock_file(fd, &fl);
        close(fd);
        return -1;
    }

    /* unlock the file */
    if (unlock_file(fd, &fl) == -1)
    {
        error_print("unlock_file");
        close(fd);
        return -1;
    }

    /* close the file */
    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }

    fprintf(stderr, "%ld number of bytes is loaded on shared memory\n", file_size);
    return 0;
}

int read_file_from_shm (const char *file_name, void **addr, int shm_fd, size_t file_size)
{
    int fd;
    char *shm_ptr;
    struct flock fl;

    /* open the file */
    if ((fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
    {
        error_print("open");
        return -1;
    }

    /* write lock the file */
    if (write_lock_file(fd, &fl) == -1)
    {
        error_print("write_lock_file");
        close(fd);
        return -1;
    }

    /* check if the file size is greater than the shared memory size */
    if (file_size > addr_size)
    {
        /* resize the shared memory */

        /* unmap the shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap");
            unlock_file(fd, &fl);
            close(fd);
            return -1;
        }
    
        /* truncate the file */
        if (ftruncate(shm_fd, file_size) == -1)
        {
            error_print("ftruncate");
            unlock_file(fd, &fl);
            close(fd);
            return -1;
        }

        /* map the file to the shared memory */
        *addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

        addr_size = file_size;
    }

    /* write file from shared memory */
    shm_ptr = (char *) *addr;

    /* set the file pointer to the beginning of the file */
    if (lseek(shm_fd, 0, SEEK_SET) == -1)
    {
        error_print("lseek");
        unlock_file(fd, &fl);
        close(fd);
        return -1;
    }
    
    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        error_print("lseek");
        unlock_file(fd, &fl);
        close(fd);
        return -1;
    }

    /* write the file */
    if (write(fd, shm_ptr, file_size) == -1)
    {
        error_print("write");
        unlock_file(fd, &fl);
        close(fd);
        return -1;
    }

    /* unlock the file */
    if (unlock_file(fd, &fl) == -1)
    {
        error_print("unlock_file");
        close(fd);
        return -1;
    }

    /* close the file */
    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }
    
    fprintf(stderr, "%ld number of bytes is read from shared memory\n", file_size);

    return 0;
}

int print_message_shm (void **addr, int shm_fd, size_t file_size)
{
    char *shm_ptr;
    size_t i;

    /* check if the file size is greater than the shared memory size */
    if (file_size > addr_size)
    {
        /* resize the shared memory */

        /* unmap the shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap");
            return -1;
        }
    
        /* truncate the file */
        if (ftruncate(shm_fd, file_size) == -1)
        {
            error_print("ftruncate");
            return -1;
        }

        /* map the file to the shared memory */
        *addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

        addr_size = file_size;
    }

    /* map the file to the shared memory */
    shm_ptr = (char *) *addr;

    /* set the file pointer to the beginning of the file */
    if (lseek(shm_fd, 0, SEEK_SET) == -1)
    {
        error_print("lseek");
        return -1;
    }

    /* print the message */
    for (i = 0; shm_ptr[i] != '\0' && i < file_size; i++)
        fprintf(stdout, "%c", shm_ptr[i]);

    return 0;
}

int write_next_2words (const char *buffer, char *word1, int *word2)
{
    char temp[MAX_BUF_LEN];

    if (write_next_word(buffer, word1) == -1)
        return -1;

    /* get the second argument */
    if (write_next_word(&buffer[strlen(word1)], temp) == -1)
    {
        /* there is no line number */
        *word2 = -1;   
        return 0;
    }    

    /* remove the newline character */
    if (temp[strlen(temp) - 1] == '\n')
        temp[strlen(temp) - 1] = '\0';

    /* convert the string to integer */
    *word2 = string_to_int(temp, strlen(temp));
    return 0;
}

int fill_arguments_w(const char *buffer, char *word1, int *word2, char *word3)
{
    char *buffer_copy;
    char *saveptr;
    char *token;

    buffer_copy = (char *) malloc(strlen(buffer) + 1);
    strcpy(buffer_copy, buffer);

    /* extract the command */
    token = strtok_r(buffer_copy, " ", &saveptr);
    if (token == NULL)
    {
        free(buffer_copy);
        return -1;
    }

    /* extract the filename */
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL)
    {
        free(buffer_copy);
        return -1;
    }
    strcpy(word1, token);

    if (saveptr[0] == '\"' && saveptr[strlen(saveptr) - 1] == '\"')
    {
        token = strtok_r(NULL, "\"", &saveptr);
        if (token == NULL)
        {
            free(buffer_copy);
            return -1;
        }

        strcpy(word3, token);
        *word2 = -1;
        free(buffer_copy);

        return 0;
    }
    
    /* extract the line number or string */
    token = strtok_r(NULL, " ", &saveptr);
    if (token != NULL)
    {
        *word2 = string_to_int(token, strlen(token));

        if (*word2 == -1)
        {
            /* it is a string */
            strcpy(word3, token);
        }
        else
        {
            /* it is a line number */
            /* extract the string */
            if (saveptr[0] == '\"' && saveptr[strlen(saveptr) - 1] == '\"')
            {
                token = strtok_r(NULL, "\"", &saveptr);
                if (token == NULL)
                {
                    free(buffer_copy);
                    return -1;
                }

                strcpy(word3, token);
            }

            else
            {
                token = strtok_r(NULL, " ", &saveptr);
                if (token == NULL)
                {
                    free(buffer_copy);
                    return -1;
                }

                strcpy(word3, token);
            }
        }

    }
    else
    {
        /* line number and string are not provided */
        free(buffer_copy);
        return -1;
    }

    free(buffer_copy);
    return 0;
}

int read_lock_file (int fd, struct flock *fl)
{
    /* set the lock type */
    fl->l_type = F_RDLCK;

    /* set the lock to the entire file */
    fl->l_whence = SEEK_SET;
    fl->l_start = 0;
    fl->l_len = 0;

    /* set the lock */
    if (fcntl(fd, F_SETLKW, fl) == -1)
    {
        error_print("fcntl, read lock");
        return -1;
    }

    return 0;
}

int write_lock_file (int fd, struct flock *fl)
{
    /* set the lock type */
    fl->l_type = F_WRLCK;

    /* set the lock to the entire file */
    fl->l_whence = SEEK_SET;
    fl->l_start = 0;
    fl->l_len = 0;

    /* set the lock */
    if (fcntl(fd, F_SETLKW, fl) == -1)
    {
        error_print("fcntl, write lock");
        return -1;
    }

    return 0;
}

int unlock_file (int fd, struct flock *fl)
{
    /* set the lock type */
    fl->l_type = F_UNLCK;

    /* set the lock to the entire file */
    fl->l_whence = SEEK_SET;
    fl->l_start = 0;
    fl->l_len = 0;

    /* set the lock */
    if (fcntl(fd, F_SETLKW, fl) == -1)
    {
        error_print("fcntl, unlock");
        return -1;
    }

    return 0;
}