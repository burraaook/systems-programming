#include "../include/mycommon.h"

sig_atomic_t signal_occured = 0;

void sig_handler (int signo)
{
    signal_occured = signo;
}

size_t addr_size = 0;

int get_request(const char *fifo_name, request_t *request);
int send_response(const char *fifo_name, response_t response);
int write_shm_to_file (const char *file_name, void **addr, int shm_fd, size_t size);
int write_file_to_shm (int file_fd, int shm_fd, void **addr, size_t size);
int write_message_shm (void **addr, int shm_fd, const char *message, size_t size);
int get_help_message (char *message, int command);
int get_file_list_shm (void **addr, size_t *size, const char *dir_name);
int write_line_to_shm (int file_fd, int shm_fd, void **addr, size_t *size, int line_num);
int write_nth_line (int file_fd, int line_num, const char *message, int shm_fd, void **addr);
int write_file_to_shm_cur (int file_fd, int shm_fd, void **addr, ssize_t *bytes_written);
int write_shm_to_file_cur (int file_fd, void **addr, ssize_t size);
ssize_t read_line(int file_fd, char *buffer, size_t buffer_size);
void log_msg (const char *msg, sem_t *sem, const char *log_file_name);

int main (int argc, char *argv[])
{
    pid_t client_pid, pid, parent_pid;
    char dir_name[MAX_FILENAME_LEN];
    char shm_name[MAX_FILENAME_LEN];
    char sem_write_name[MAX_FILENAME_LEN];
    char log_sem_name[MAX_FILENAME_LEN];
    char log_file_name[MAX_FILENAME_LEN];
    char file_name[MAX_FILENAME_LEN];
    char svc_fifo_name[MAX_FILENAME_LEN], cl_fifo_name[MAX_FILENAME_LEN];
    char buffer[MAX_BUF_LEN];
    int shm_fd, file_fd;
    struct sigaction sa;
    request_t request;
    response_t response;
    sem_t *log_sem, *write_sem;
    void *addr;
    char *message;

    response.flag = SV_SUCCESS;

    if (argc != 3)
        error_exit("wrong number of arguments");

    parent_pid = getppid();
    /* get log file name */
    snprintf(log_file_name, MAX_FILENAME_LEN, LOG_FILE_NAME, parent_pid);

    /* first argument is the client pid */
    client_pid = string_to_pid(argv[1], strlen(argv[1]));

    /* second argument is the directory name */
    strcpy(dir_name, argv[2]);

    /* set printf to be unbuffered */
    setbuf(stdout, NULL);

    /* current pid */
    pid = getpid();

    /* set the signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &sig_handler;
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    block_all_signals();
    /* open semaphore */
    snprintf(sem_write_name, MAX_FILENAME_LEN, WRITE_SEM_NAME, parent_pid);
    /* fprintf(stderr, "sem_write_name: %s\n", sem_write_name); */
    if ((write_sem = sem_open(sem_write_name, 0)) == SEM_FAILED)
        error_exit("sem_open2");
    

    /* open log semaphore */
    snprintf(log_sem_name, MAX_FILENAME_LEN, LOG_SEM_NAME, parent_pid);

    if ((log_sem = sem_open(log_sem_name, 0)) == SEM_FAILED)
        error_exit("sem_open - log_sem");
    unblock_all_signals();

    /* fifo name */
    snprintf(svc_fifo_name, MAX_FILENAME_LEN, "%s_%d_%d", SVC_FIFO_NAME, client_pid, pid);
    snprintf(cl_fifo_name, MAX_FILENAME_LEN, "%s_%d", CL_FIFO_NAME, client_pid);

    /* shm name */
    snprintf(shm_name, MAX_FILENAME_LEN, "%s_%d", SHM_NAME, client_pid);

    block_all_signals();
    /* open/create shared memory */
    snprintf(shm_name, MAX_FILENAME_LEN, "%s_%d", SHM_NAME, client_pid);
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
    
    snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> Client %d connected to child server\n", pid, client_pid);
    log_msg(buffer, log_sem, log_file_name);

    while(1)
    {
        if (signal_occured > 0)
        {
            switch (signal_occured)
            {
                case SIGINT:
                    /* send sigint to the client */
                    kill(client_pid, SIGINT);
                    snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> SIGINT received. Terminating..\n", pid);
                    log_msg(buffer, log_sem, log_file_name);
                    sem_close(log_sem);
                    sem_close(write_sem);
                    exit(EXIT_SUCCESS);                    
                break;

                case SIGTERM: 
                    
                    /* send sigint to the client */
                    kill(client_pid, SIGINT);
                    snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> SIGTERM received. Terminating..\n", pid);
                    log_msg(buffer, log_sem, log_file_name);
                    sem_close(log_sem);
                    sem_close(write_sem);
                    exit(EXIT_SUCCESS);
                break;

                case SIGQUIT:
                    
                    /* send sigint to the client */
                    kill(client_pid, SIGINT);
                    snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> SIGQUIT received. Terminating..\n", pid);
                    log_msg(buffer, log_sem, log_file_name);
                    sem_close(log_sem);
                    sem_close(write_sem);
                    exit(EXIT_SUCCESS);
                break;
            }

            signal_occured = 0;       
        }

        /* this is not busy waiting, continue if there are any error, 
        it is blocked if there are no request. */
        if (get_request(svc_fifo_name, &request) == -1)
            continue;

        switch (request.command)
        {
            case CMD_HELP:
                response.flag = SV_SUCCESS;
                // malloc_flag = 1;
                message = malloc(sizeof(char) * MAX_MESSAGE_LEN);

                if (message == NULL)
                {
                    error_print("malloc");
                    response.flag = SV_FAILURE;
                    // malloc_flag = 0;
                    send_response(cl_fifo_name, response);
                    continue;
                }

                snprintf(message, MAX_MESSAGE_LEN,
                "\n   Available Comments are:\nconnect, tryConnect, help, list, readF, writeT, upload, download, quit, killServer\n\n");
                response.file_size = strlen(message) + 1;                    

                /* write the message to shared memory */
                if (write_message_shm (&addr, shm_fd, message, strlen(message) + 1) == -1)
                {
                    fprintf(stderr, "Help failed\n");
                    free(message);
                    response.flag = SV_FAILURE;
                    send_response(cl_fifo_name, response);
                    continue;
                }

                if (send_response(cl_fifo_name, response) == -1)
                    fprintf(stderr, "Help failed\n");

                free(message);

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> help command is executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;

            case CMD_READF:
                response.flag = SV_SUCCESS;

                /* get file name */
                snprintf(file_name, MAX_FILENAME_LEN + 1, "%s/", dir_name);
                strncat(file_name, request.file_name, MAX_FILENAME_LEN - strlen(file_name));

                block_all_signals();
                /* open file */
                if ((file_fd = open(file_name, O_RDONLY)) == -1)
                    response.flag = SV_FAILURE;
                unblock_all_signals();

                block_all_signals();
                /* enter critical section for read */
                sem_wait(write_sem);
                sem_post(write_sem);
                
                if (response.flag != SV_FAILURE &&
                        write_line_to_shm(file_fd, shm_fd, &addr, &response.file_size, request.arg1) == -1)
                {
                    error_print_custom("readF failed");
                    response.flag = SV_FAILURE;
                }
                unblock_all_signals();

                /* send response to the client */
                if ((send_response(cl_fifo_name, response)) == -1)
                    error_print_custom("readF failed");

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> readF command is executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;

            case CMD_LIST:

                response.flag = SV_SUCCESS;

                /* get file list */
                if (get_file_list_shm(&addr, &response.file_size, dir_name) == -1)
                {
                    fprintf(stderr, "List failed\n");
                    response.flag = SV_FAILURE;
                    send_response(cl_fifo_name, response);
                }

                /* send response to the client */
                if (send_response(cl_fifo_name, response) == -1)
                    fprintf(stderr, "List failed\n");

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> list command executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;
            case CMD_UPLOAD:

                response.flag = SV_SUCCESS;

                /* get file name */
                snprintf(file_name, MAX_FILENAME_LEN+1, "%s/", dir_name);
                strncat(file_name, request.file_name, MAX_FILENAME_LEN - strlen(file_name));

                block_all_signals();
                /* enter critical section */
                sem_wait(write_sem);
                
                /* read from shared memory, write to file */
                if (write_shm_to_file(file_name, &addr, shm_fd, request.file_size) == -1)
                {
                    fprintf(stderr, "Upload failed\n");
                    response.flag = SV_FAILURE;
                }

                /* exit critical section */
                sem_post(write_sem);
                unblock_all_signals();

                /* send response to the client */
                if (send_response(cl_fifo_name, response) == -1)
                    fprintf(stderr, "Upload failed\n");

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> upload command is executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;

            case CMD_DOWNLOAD:

                /* get file name */
                snprintf(file_name, MAX_FILENAME_LEN+1, "%s/", dir_name);
                strncat(file_name, request.file_name, MAX_FILENAME_LEN - strlen(file_name));
                response.flag = SV_SUCCESS;

                /* open file */
                if ((file_fd = open(file_name, O_RDONLY)) == -1)
                {
                    response.flag = SV_FAILURE;
                    fprintf(stderr, "Download failed\n");
                    /* send response to the client */
                    send_response(cl_fifo_name, response);
                    continue;
                }

                block_all_signals();
                /* enter critical section for reading*/
                sem_wait(write_sem);
                sem_post(write_sem);

                /* get file size */
                response.file_size = lseek(file_fd, 0, SEEK_END);

                /* read from file, write to shared memory */
                if (write_file_to_shm(file_fd, shm_fd, &addr, response.file_size) == -1)
                {
                    response.flag = SV_FAILURE;
                    fprintf(stderr, "Download failed\n");
                    close(file_fd);
                    /* send response to the client */
                    send_response(cl_fifo_name, response);
                }
                unblock_all_signals();
                
                /* send response to the client */
                if (send_response(cl_fifo_name, response) == -1)
                    fprintf(stderr, "Download failed\n");

                /* close file */
                close(file_fd);

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> download command is executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;

            case CMD_WRITET:

                response.flag = SV_SUCCESS;

                /* get file name */
                snprintf(file_name, MAX_FILENAME_LEN+1, "%s/", dir_name);
                strncat(file_name, request.file_name, MAX_FILENAME_LEN - strlen(file_name));

                /* open file, create if it does not exist */
                if ((file_fd = open(file_name, O_RDWR | O_CREAT, 0666)) == -1)
                {
                    error_print("open1");
                    response.flag = SV_FAILURE;
                    send_response(cl_fifo_name, response);
                }

                block_all_signals();
                /* enter critical section */
                sem_wait(write_sem);
                
                /* write to file */
                if (write_nth_line(file_fd, request.arg1, request.message, shm_fd, &addr) == -1)
                {
                    fprintf(stderr, "writeT failed\n");
                    response.flag = SV_FAILURE;
                }
                /* close file */
                close(file_fd);
                
                /* exit critical section */
                sem_post(write_sem);
                unblock_all_signals();
                
                if (response.flag == SV_FAILURE)
                {
                    send_response(cl_fifo_name, response);
                    continue;
                }

                /* send response to the client */
                if (send_response(cl_fifo_name, response) == -1)
                    fprintf(stderr, "writeT failed\n");

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> writeT command is executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;

            default:

                if (request.command > 16)
                {
                    fprintf(stderr, "Unknown command\n");
                    continue;
                }

                response.flag = SV_SUCCESS;
                message = malloc(sizeof(char) * MAX_MESSAGE_LEN);
                
                if (message == NULL)
                {
                    error_print("malloc");
                    response.flag = SV_FAILURE;
                    send_response(cl_fifo_name, response);
                    continue;
                }

                if (get_help_message(message, request.command) == -1)
                {
                    error_print_custom("get_help_message failed");
                    free(message);
                    response.flag = SV_FAILURE;
                    send_response(cl_fifo_name, response);
                    continue;
                } 

                block_all_signals();
                /* write the message to shared memory */
                if (write_message_shm (&addr, shm_fd, message, strlen(message) + 1) == -1)
                {
                    fprintf(stderr, "Help failed\n");
                    free(message);
                    response.flag = SV_FAILURE;
                    send_response(cl_fifo_name, response);
                    unblock_all_signals();
                    continue;
                }
                unblock_all_signals();

                response.file_size = strlen(message) + 1;
                /* send response to the client */
                if (send_response(cl_fifo_name, response) == -1)
                    fprintf(stderr, "Help failed\n");

                free(message);

                snprintf(buffer, MAX_BUF_LEN, "CHILD SERVER: %d >> help command is executed for client %d\n", pid, client_pid);
                log_msg(buffer, log_sem, log_file_name);
            break;
        }
    }

    return 0;
}

int write_shm_to_file (const char *file_name, void **addr,int shm_fd, size_t size)
{
    int fd;
    char *p;
    struct flock lock;

    if ((fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
    {
        if (errno == ENOENT)
        {
            if ((fd = open(file_name, O_WRONLY | O_CREAT, 0666)) == -1)
            {
                error_print("open - write_shm_to_file");
                fprintf(stderr, "file name: %s", file_name);
                return -1;
            }
        }
        else
        {
            error_print("open - write_shm_to_file");
            return -1;
        }
        return -1;
    }

    /* check if shm is big enough */
    if (addr_size < size)
    {
        /* unmap shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap1");
            return -1;
        }

        /* resize shared memory */
        if (ftruncate(shm_fd, size) == -1)
        {
            error_print("ftruncate");
            return -1;
        }

        /* map shared memory */
        if ((*addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        {
            error_print("mmap2");
            return -1;
        }

        addr_size = size;
    }

    p = (char *) *addr;

    memset(&lock, 0, sizeof(lock));
    /* set lock using fcntl */
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(fd, F_SETLKW, &lock) == -1)
    {
        error_print("fcntl");
        close(fd);
        return -1;
    }

    if (write(fd, p, size) == -1)
    {
        error_print("write3");
        close(fd);
        return -1;
    }

    /* release lock */
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) == -1)
    {
        error_print("fcntl");
        close(fd);
        return -1;
    }

    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }
    
    return 0;
}

int write_file_to_shm (int file_fd, int shm_fd, void **addr, size_t size)
{
    char* p;

    /* check if the shared memory is big enough */
    if (addr_size < size)
    {
        /* unmap shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap1");
            return -1;
        }

        /* resize shared memory */
        if (ftruncate(shm_fd, size) == -1)
        {
            error_print("ftruncate");
            return -1;
        }

        /* map shared memory */
        if ((*addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        {
            error_print("mmap2");
            return -1;
        }

        addr_size = size;
    }

    p = (char *) *addr;

    /* seek to the beginning of the file */
    if (lseek(file_fd, 0, SEEK_SET) == -1)
    {
        error_print("lseek");
        return -1;
    }

    /* read from file, write to shared memory */
    if (read(file_fd, p, size) == -1)
    {
        error_print("read");
        return -1;
    }

    return 0;
}

int write_message_shm (void **addr, int shm_fd, const char *message, size_t size)
{
    char *p;

    /* check if the shared memory is big enough */
    if (addr_size < size)
    {
        /* unmap shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap1");
            return -1;
        }

        /* resize shared memory */
        if (ftruncate(shm_fd, size) == -1)
        {
            error_print("ftruncate");
            return -1;
        }

        /* map shared memory */
        if ((*addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        {
            error_print("mmap2");
            return -1;
        }

        addr_size = size;
    }

    p = (char *) *addr;

    /* write to shared memory */
    if (memcpy(p, message, size) == NULL)
    {
        error_print("memcpy");
        return -1;
    }

    return 0;
}

int get_help_message (char *message, int command)
{
    switch (command)
    {
            case CMD_HELP_READF:
                snprintf(message, MAX_MESSAGE_LEN,
                "\n   readF <file> <line #>\n         ");

                strcat(message, "display the #th line of the <file>, returns with an error if <file> does not exist\n\n");
            break;

            case CMD_HELP_WRITET:

                snprintf(message, MAX_MESSAGE_LEN, "\n  writeT <file> <line #> <string>\n         ");
                
                strcat(message, "write <string> to the #th line of the <file>, if <line #> is not given writes end of the file\n\n");
            break;

            case CMD_HELP_UPLOAD:
                    
                snprintf(message, MAX_MESSAGE_LEN, "\n  upload <file>\n         ");
                
                strcat(message, "upload <file> to the server directory.\n\n");
            break;

            case CMD_HELP_DOWNLOAD:

                snprintf(message, MAX_MESSAGE_LEN, "\n  download <file>\n         ");
                
                strcat(message, "download <file> from the server directory.\n\n");
            break;

            case CMD_HELP_QUIT:
                    
                snprintf(message, MAX_MESSAGE_LEN, "\n  quit\n         ");
                
                strcat(message, "write to log file and quit.\n\n");
            break;

            case CMD_HELP_KILLSERVER:
                    
                snprintf(message, MAX_MESSAGE_LEN, "\n  killServer\n         ");
                
                strcat(message, "write to log file and kill the server.\n\n");
            break;

            case CMD_HELP_LIST:
                        
                snprintf(message, MAX_MESSAGE_LEN, "\n  list\n         ");
                
                strcat(message, "display list the files in the server directory.\n\n");
            break;

            default:
                return -1;        
    }

    return 0;
}

int get_file_list_shm(void **addr, size_t *size, const char *dir_name)
{
    int pipefd[2];
    pid_t pid;
    int status;
    char buffer[1024];
    ssize_t read_bytes;

    /* create pipe */
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    /* child process */
    else if (pid == 0) {

        /* close read end */
        close(pipefd[0]);

        /* redirect stdout to pipe */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        /* execute ls <dir_name> */
        execlp("ls", "ls", dir_name, NULL);

        perror("execlp");
        exit(1);
    }
    else {
        /* parent process */
        /* close write end */
        close(pipefd[1]);

        /* wait for child */
        if (wait(&status) == -1) {
            perror("wait");
            return -1;
        }

        /* check child status */
        if (!WIFEXITED(status)) {
            perror("failed to execute ls");
            return -1;
        }

        /* read from pipe */
        read_bytes = read(pipefd[0], buffer, sizeof(buffer));
        if (read_bytes == -1) {
            perror("read");
            return -1;
        }

        /* close read end */
        close(pipefd[0]);

        /* write to shared memory */
        memcpy(*addr, buffer, read_bytes);
        *size = read_bytes;

        return 0;
    }
}

int write_line_to_shm (int file_fd, int shm_fd, void **addr, size_t *size, int line_num)
{
    off_t file_size, start;
    ssize_t read_bytes, i;
    char *line_buffer;
    int cur, line_count;
    size_t line_length;

    /* get file size */
    if ((file_size = lseek(file_fd, 0, SEEK_END)) == -1)
    {
        error_print("lseek");
        return -1;
    }

    /* set file pointer to the beginning of the file */
    if (lseek(file_fd, 0, SEEK_SET) == -1)
    {
        error_print("lseek");
        return -1;
    }

    /* if line_num is leq 0, write whole file to shared memory */
    if (line_num <= 0)
    {
        /* write whole file to shared memory */
        if (write_file_to_shm(file_fd, shm_fd, addr, file_size) == -1)
        {
            error_print_custom("write_file_to_shm");
            return -1;
        }
        *size = file_size;
        return 0;
    }

    /* allocate memory for line buffer */
    if ((line_buffer = malloc(sizeof(char) * file_size)) == NULL)
    {
        error_print("malloc");
        return -1;
    }

    /* read file contents to line buffer */
    if ((read_bytes = read(file_fd, line_buffer, file_size)) == -1)
    {
        error_print("read");
        free(line_buffer);
        return -1;
    }

    /* count the number of lines */
    line_count = 0;
    for (i = 0; i < read_bytes; i++)
        if (line_buffer[i] == '\n')
            line_count++;


    /* check if line number is invalid */
    if (line_num > line_count || line_num < 0)
    {
        /* write whole file to shared memory */
        if (write_file_to_shm(file_fd, shm_fd, addr, file_size) == -1)
        {
            error_print_custom("write_file_to_shm");
            free(line_buffer);
            return -1;
        }
        free(line_buffer);
        *size = file_size;
        return 0;
    }

    /* find the start, and length of the line */
    start = 0;
    line_length = 0;
    cur = 1;
    for (i = 0; i < read_bytes; ++i) 
    {
        if (line_buffer[i] == '\n') 
        {
            cur++;
            if (cur > line_num) 
                break;
            start = i + 1;
        } 
        else if (cur == line_num)
            line_length++;
    }

    /* write the line to shared memory */
    
    /* check if the shared memory is big enough */
    
    if (addr_size < line_length)
    {
        /* unmap shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap1");
            free(line_buffer);
            return -1;
        }

        /* resize shared memory */
        if (ftruncate(shm_fd, line_length) == -1)
        {
            error_print("ftruncate");
            free(line_buffer);
            return -1;
        }

        /* map shared memory */
        if ((*addr = mmap(NULL, line_length, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        {
            error_print("mmap2");
            free(line_buffer);
            return -1;
        }

        addr_size = line_length;
    }

    *size = line_length;

    /* write to shared memory */
    if (memcpy(*addr, line_buffer + start, line_length) == NULL)
    {
        error_print("memcpy");
        free(line_buffer);
        return -1;
    }

    free(line_buffer);

    return 0;
}

ssize_t read_line(int file_fd, char *buffer, size_t buffer_size)
{
    ssize_t bytes_read = 0;
    ssize_t total_bytes_read = 0;
    ssize_t max_bytes_read = (ssize_t) buffer_size - 1;
    while (total_bytes_read < max_bytes_read) {
        bytes_read = read(file_fd, buffer + total_bytes_read, 1);
        if (bytes_read == -1)
            return -1;

        if (bytes_read == 0)
            break;

        total_bytes_read += bytes_read;

        if (buffer[total_bytes_read - 1] == '\n')
            break;
    }

    buffer[total_bytes_read] = '\0';
    return total_bytes_read;
}


int write_nth_line(int file_fd, int line_num, const char *message, int shm_fd, void **addr)
{
    /* declare variables */
    off_t file_offset, last_offset;
    ssize_t bytes_written, bytes_read, line_bytes_read, old_offset, bytes_written_shm;
    int current_line;
    char line_buffer[1024];
    char ch;
    ssize_t cur = 0;

    /* line_num is -1, append the message to the end of the file */
    if (line_num == -1) {
        /* move the file offset to the end */
        file_offset = lseek(file_fd, 0, SEEK_END);
        if (file_offset == -1)
            return -1;

        /* write the message */
        bytes_written = write(file_fd, message, strlen(message));
        if (bytes_written == -1)
            return -1;

        /* append a newline character if needed */
        if (message[strlen(message) - 1] != '\n') {
            bytes_written = write(file_fd, "\n", 1);
            if (bytes_written == -1)
                return -1;
        }

        return 0;
    }

    /* move the file offset to the beginning */
    file_offset = lseek(file_fd, 0, SEEK_SET);
    if (file_offset == -1)
        return -1;

    /* iterate over the lines until the desired line */
    current_line = 1;
    while (current_line < line_num) {
        bytes_read = read(file_fd, &ch, sizeof(char));
        if (bytes_read == -1)
            return -1;
        
        if (bytes_read == 0) {
            /* reached the end of the file, append the message */
            /* move the file offset to the end */
            file_offset = lseek(file_fd, 0, SEEK_END);
            if (file_offset == -1)
                return -1;

            /* write the message */
            bytes_written = write(file_fd, message, strlen(message));
            if (bytes_written == -1)
                return -1;

            /* append a newline character if needed */
            if (message[strlen(message) - 1] != '\n') {
                bytes_written = write(file_fd, "\n", 1);
                if (bytes_written == -1)
                    return -1;
            }

            return 0;
        }

        if (ch == '\n')
            current_line++;
    }

    /* read the remaining contents of the line */
    old_offset = lseek(file_fd, 0, SEEK_CUR);
    line_bytes_read = read_line(file_fd, line_buffer, sizeof(line_buffer));
    if (line_bytes_read == -1)
        return -1;

    /* truncate the line if necessary */
    if ((ssize_t ) strlen(message) < line_bytes_read) 
    {
        /* move the file offset back to the beginning of the line */
        file_offset = lseek(file_fd, old_offset, SEEK_SET);
        if (file_offset == -1)
            return -1;

        /* write the updated line */
        bytes_written = write(file_fd, message, strlen(message));
        if (bytes_written == -1)
            return -1;

        /* append a newline character if needed */
        if (message[strlen(message) - 1] != '\n') {
            bytes_written = write(file_fd, "\n", 1);
            if (bytes_written == -1)
                return -1;
        }
        
        last_offset = lseek(file_fd, 0, SEEK_CUR);
        
        /* discard remaining contents till newline comes */
        while (1) {
            bytes_read = read(file_fd, &ch, sizeof(char));
            if (bytes_read == -1)
                return -1;

            if (bytes_read == 0)
                break;

            if (ch == '\n')
                break;
        }
        cur = lseek(file_fd, 0, SEEK_CUR);
        if (cur == -1)
            return -1;

        bytes_written_shm = 0;
        /* write the remaining contents after the line to the shared memory */
        if (write_file_to_shm_cur(file_fd, shm_fd, addr, &bytes_written_shm) == -1)
            return -1;

        /* truncate the fill till the end of modified line */
        if (ftruncate(file_fd, last_offset) == -1)
            return -1;
        
        lseek(file_fd, 0, SEEK_END);
        
        /* write the remaining contents from the shared memory */
        if (write_shm_to_file_cur(file_fd, addr, bytes_written_shm) == -1)
            return -1;
        
        return 0;
    } 
    else
    {
        bytes_written_shm = 0;
        /* save the remaining contents to the shared memory */
        if (write_file_to_shm_cur(file_fd, shm_fd, addr, &bytes_written_shm) == -1)
            return -1;

        /* move the file offset back to the beginning of the line */
        file_offset = lseek(file_fd, old_offset, SEEK_SET);

        /* write the message */
        bytes_written = write(file_fd, message, strlen(message));

        /* append a newline character if needed */
        if (message[strlen(message) - 1] != '\n') {
            bytes_written = write(file_fd, "\n", 1);
            if (bytes_written == -1)
                return -1;
        }

        /* write the remaining contents from the shared memory */
        if (write_shm_to_file_cur(file_fd, addr, bytes_written_shm) == -1)
            return -1;

        return 0;
    }

    return 0;
}

int write_file_to_shm_cur (int file_fd, int shm_fd, void **addr, ssize_t *bytes_written)
{
    size_t old_offset, file_size;

    /* save the current file offset */
    old_offset = lseek(file_fd, 0, SEEK_CUR);

    /* get file size */
    file_size = lseek(file_fd, 0, SEEK_END);

    if (file_size > old_offset)
        file_size = file_size - old_offset;
    else
        file_size = 0;
    
    /* set file offset to the beginning */
    lseek(file_fd, old_offset, SEEK_SET);

    /* check if the shared memory is big enough */
    if (addr_size < file_size)
    {
        /* unmap shared memory */
        if (munmap(*addr, addr_size) == -1)
        {
            error_print("munmap1");
            return -1;
        }

        /* resize shared memory */
        if (ftruncate(shm_fd, file_size) == -1)
        {
            error_print("ftruncate11");
            return -1;
        }

        /* map shared memory */
        if ((*addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        {
            error_print("mmap2");
            return -1;
        }

        addr_size = file_size;
    }

    /* write to shared memory */
    if ((*bytes_written = read(file_fd, *addr, file_size)) == -1)
    {
        error_print("read");
        return -1;
    }

    return 0;
}

int write_shm_to_file_cur (int file_fd, void **addr, ssize_t size)
{
    if (size == 0 || size > (ssize_t) addr_size)
        return 0;

    /* write to file */
    if (write(file_fd, *addr, size) == -1)
    {
        error_print("write");
        return -1;
    }
    return 0;
}

int get_request(const char *fifo_name, request_t *request)
{
    int fd;

    /* open fifo */
    if ((fd = open(fifo_name, O_RDONLY)) == -1)
    {
        if (errno != EINTR)
            error_print("open1w");
        return -1;
    }

    /* read request */
    if (read(fd, request, sizeof(request_t)) == -1)
    {
        error_print("read");
        return -1;
    }

    /* close fifo */
    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }

    return 0;
}

int send_response(const char *fifo_name, response_t response)
{
    int fd;

    /* open fifo */
    if ((fd = open(fifo_name, O_WRONLY)) == -1)
    {
        if (errno != EINTR)
            error_print("open2");
        return -1;
    }

    /* write response */
    if (write(fd, &response, sizeof(response_t)) == -1)
    {
        error_print("write");
        return -1;
    }

    /* close fifo */
    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }

    return 0;
}

void log_msg (const char *msg, sem_t *sem, const char *log_file_name)
{
    int fd;

    /* enter critical section */
    if (sem_wait(sem) == -1)
    {
        error_print("sem_wait");
        return;
    }

    /* open log file */
    if ((fd = open(log_file_name, O_WRONLY | O_APPEND)) == -1)
    {
        error_print("open");
        sem_post(sem);
        return;
    }

    /* write to log file */
    if (write(fd, msg, strlen(msg)) == -1)
    {
        error_print("write");
        sem_post(sem);
        return;
    }

    /* close log file */
    if (close(fd) == -1)
    {
        error_print("close");
        sem_post(sem);
        return;
    }

    /* exit critical section */
    if (sem_post(sem) == -1)
    {
        error_print("sem_post");
        return;
    }
}