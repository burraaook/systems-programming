#include "include/mycommon.h"

/* 10 GB is the maximum file size */
const size_t MAX_FILE_SIZE = (size_t) 10*1024*1024*1024;
const char DEFAULT_BYTE = '0';

int main (int argc, char *argv[])
{
    /* declare variables */
    size_t num_bytes, i;
    short int x_exist;
    mode_t mode;
    int flags;
    int fd;

    /* check the arguments */
    if (argc < 3 || argc > 4) 
        error_exit_custom("This program requires 2 or 3 arguments");
    else 
    {
        /* first argument is filename no need to check */
        /* second argument is byte to write */
        num_bytes = string_to_uint(argv[2], strlen(argv[2]));
        
        /* check file size */
        if (num_bytes > MAX_FILE_SIZE)
            error_exit_custom("File size is too large! Max file size is 10GB");

        /* check third argument */
        if (argc == 4)
        {
            /* third argument is x or not */
            if (strcmp(argv[3], "x") != 0)
                error_exit_custom("Third argument must be x or not exist");
            x_exist = 1;
        }
        else
            x_exist = 0;
    }

    /* determine mode and flags */

    /* 0666 is rw-rw-rw- */
    mode = 0666;

    if (x_exist)
        flags = O_WRONLY | O_CREAT;
    else
        flags = O_WRONLY | O_CREAT | O_APPEND;

    /* open file */
    fd = open(argv[1], flags, mode);
    if (fd == -1)
        error_exit("open");

    /* write bytes */
    if (x_exist)
    {
        for (i = 0; i < num_bytes; ++i)
        {
            if(lseek(fd, 0, SEEK_END) == -1)
                error_exit("lseek");

            if(write(fd, &DEFAULT_BYTE, 1) == -1)
                error_exit("write");
        }
    }
    else /* append mode */ 
    {
        for (i = 0; i < num_bytes; ++i)
        {
            if(write(fd, &DEFAULT_BYTE, 1) == -1)
                error_exit("write");
        }
    }

    /* close file */
    if (close(fd) == -1)
        error_exit("close");
    
    return 0;
}

