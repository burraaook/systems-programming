#include "../include/mycommon.h"
#include "../include/command_impl.h"

int write_shm_to_file (const char *file_name, const char *shm_name, size_t size)
{
    int fd, shm_fd;
    char *p;
    void *addr;


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

    /* open shared memory */
    if ((shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, 0666)) == -1)
    {
        error_print("shm_open");
        return -1;
    }

    /* truncate shared memory */
    if (ftruncate(shm_fd, size) == -1)
    {
        error_print("ftruncate1");
        close(fd);
        close(shm_fd);
        return -1;
    }

    /* map shared memory */
    if ((addr = mmap(NULL, size, PROT_READ, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    {
        error_print("mmap");
        close(fd);
        return -1;
    }

    p = (char *) addr;

    lseek(fd, 0, SEEK_SET);

    if (write(fd, p, size) == -1)
    {
        error_print("write3");
        close(fd);
        return -1;
    }

    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }

    free_shm(&addr, shm_fd, size);

    return 0;
}

int write_file_to_shm (int file_fd, const char *shm_name, size_t size)
{
    char* p;
    int shm_fd;
    void *addr;

    /* open shared memory */
    if ((shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
    {
        error_print("shm_open");
        return -1;
    }

    /* truncate shared memory */
    if (ftruncate(shm_fd, size) == -1)
    {
        error_print("ftruncate");
        return -1;
    }

    /* map shared memory */
    if ((addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    {
        error_print("mmap");
        return -1;
    }

    p = (char *) addr;

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

    free_shm(&addr, shm_fd, size);

    return 0;
}

int write_message_shm (const char *shm_name, const char *message, size_t size)
{
    char *p;
    int shm_fd;
    void *addr;

    /* open shared memory */
    if ((shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
    {
        error_print("shm_open");
        return -1;
    }

    /* truncate shared memory */
    if (ftruncate(shm_fd, size) == -1)
    {
        error_print("ftruncate");
        return -1;
    }

    /* map shared memory */
    if ((addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    {
        error_print("mmap");
        return -1;
    }

    p = (char *) addr;

    /* write to shared memory */
    if (memcpy(p, message, size) == NULL)
    {
        error_print("memcpy");
        return -1;
    }

    free_shm(&addr, shm_fd, size);

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

int get_file_list_shm (const char *shm_name, size_t *size, const char *dir_name)
{
    int pipefd[2];
    pid_t pid;
    int status, shm_fd;
    char buffer[1024];
    ssize_t read_bytes;
    void *addr;

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

        /* open shared memory */
        if ((shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
        {
            error_print("shm_open");
            return -1;
        }

        /* truncate shared memory */
        if (ftruncate(shm_fd, 1024) == -1)
        {
            error_print("ftruncate");
            return -1;
        }

        /* map shared memory */
        if ((addr = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        {
            error_print("mmap");
            return -1;
        }

        /* write to shared memory */
        memcpy(addr, buffer, read_bytes);
        *size = read_bytes;

        /* unmap shared memory */
        munmap(addr, 1024);

        /* close shared memory */
        close(shm_fd);

        return 0;
    }
}

int write_line_to_shm (int file_fd, const char *shm_name, size_t *size, int line_num)
{
    off_t file_size, start;
    ssize_t read_bytes, i;
    char *line_buffer;
    int cur, line_count, shm_fd;
    size_t line_length;
    void *addr;

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
        if (write_file_to_shm(file_fd, shm_name, file_size) == -1)
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
        if (write_file_to_shm(file_fd, shm_name, file_size) == -1)
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
    
    /* open shared memory */
    if ((shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
    {
        error_print("shm_open");
        free(line_buffer);
        return -1;
    }

    /* truncate shared memory */
    if (ftruncate(shm_fd, line_length) == -1)    
    {
        error_print("ftruncate");
        free(line_buffer);
        return -1;
    }

    /* map shared memory */
    if ((addr = mmap(NULL, line_length, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    {
        error_print("mmap");
        free(line_buffer);
        return -1;
    }

    *size = line_length;

    /* write to shared memory */
    if (memcpy(addr, line_buffer + start, line_length) == NULL)
    {
        error_print("memcpy");
        free(line_buffer);
        return -1;
    }

    free(line_buffer);
    free_shm(&addr, shm_fd, line_length);

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


int write_nth_line (int file_fd, int line_num, const char *message, const char *shm_name)
{
    /* declare variables */
    off_t file_offset, last_offset;
    ssize_t bytes_written, bytes_read, line_bytes_read, old_offset, bytes_written_shm;
    int current_line, shm_fd;
    char line_buffer[1024];
    char ch;
    ssize_t cur = 0;
    void *addr;
    size_t addr_size = SHM_DEFAULT_SIZE;

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

    /* open shared memory */
    if ((shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
    {
        error_print("shm_open");
        return -1;
    }

    /* truncate shared memory */
    if (ftruncate(shm_fd, SHM_DEFAULT_SIZE) == -1)
    {
        error_print("ftruncate");
        return -1;
    }

    /* map shared memory */
    if ((addr = mmap(NULL, SHM_DEFAULT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
    {
        error_print("mmap");
        return -1;
    }

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
        if (write_file_to_shm_cur(file_fd, shm_fd, &addr, &bytes_written_shm, &addr_size) == -1)
            return -1;

        /* truncate the fill till the end of modified line */
        if (ftruncate(file_fd, last_offset) == -1)
            return -1;
        
        lseek(file_fd, 0, SEEK_END);
        
        /* write the remaining contents from the shared memory */
        if (write_shm_to_file_cur(file_fd, &addr, bytes_written_shm, &addr_size) == -1)
            return -1;
    } 
    else
    {
        bytes_written_shm = 0;
        /* save the remaining contents to the shared memory */
        if (write_file_to_shm_cur(file_fd, shm_fd, &addr, &bytes_written_shm, &addr_size) == -1)
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
        if (write_shm_to_file_cur(file_fd, &addr, bytes_written_shm, &addr_size) == -1)
            return -1;
    }


    free_shm(&addr, shm_fd, SHM_DEFAULT_SIZE);    
    return 0;
}

int write_file_to_shm_cur (int file_fd, int shm_fd, void **addr, ssize_t *bytes_written, size_t *addr_size)
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
    if (*addr_size < file_size)
    {
        /* unmap shared memory */
        if (munmap(*addr, *addr_size) == -1)
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

        *addr_size = file_size;
    }

    /* write to shared memory */
    if ((*bytes_written = read(file_fd, *addr, file_size)) == -1)
    {
        error_print("read");
        return -1;
    }

    return 0;
}

int write_shm_to_file_cur (int file_fd, void **addr, ssize_t size, size_t *addr_size)
{
    if (size == 0 || size > (ssize_t) (*addr_size))
        return 0;

    /* write to file */
    if (write(file_fd, *addr, size) == -1)
    {
        error_print("write");
        return -1;
    }
    return 0;
}

int free_shm (void **addr, int shm_fd, size_t size)
{
    /* unmap shared memory */
    if (munmap(*addr, size) == -1)
    {
        error_print("munmap1");
        return -1;
    }

    /* close shared memory */
    if (close(shm_fd) == -1)
    {
        error_print("close");
        return -1;
    }

    return 0;
}