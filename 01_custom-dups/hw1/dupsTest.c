#include "include/customdups.h"
#include "include/mycommon.h"

int main (int argc, char* argv[])
{
    /* declarations */
    int i;
    int fd1, fd2, fd3, fd4;
    int invalid_fd;
    char *filename1, *filename2;

    /* check if filename is provided */
    if (argc > 3)
        error_exit_custom("This program requires 2 argument as file names");
    else if (argc > 1) {
        filename1 = argv[1];
        filename2 = argv[2];
    }
    else {
        filename1 = "part2_file1.txt";
        filename2 = "part2_file2.txt";
    }
    
    /* open file */
    fd1 = open(filename1, O_WRONLY | O_CREAT, 0666);
    if (fd1 == -1)
        error_exit("open");

    fprintf(stdout, "\n%s file is opened with fd1 = %d\n", filename1, fd1);

    /* duplicate file descriptor */
    fd2 = custom_dup(fd1);
    if (fd2 == -1)
        error_exit("custom_dup");

    fprintf(stdout, "\nfd1 is duplicated to fd2 = %d\n", fd2);

    /* write to file */
    fprintf(stdout, "\nwriting to file %s with fd1 and fd2\n", filename1);
    if (write(fd1, "1- fd1\n", 7) == -1)
        perror("write");
    if (write(fd2, "2- fd2\n", 7) == -1)
        perror("write");

    fd3 = custom_dup2(fd1, 15);
    if (fd3 == -1)
        error_exit("custom_dup2");
    
    fprintf(stdout, "\nfd1 is duplicated using custom_dup2(fd1, 15) to fd3 = %d\n", fd3);

    /* write to file */
    fprintf(stdout, "\nwriting to file %s with fd1, fd2 and fd3\n", filename1);
    if (write(fd1, "3- fd1\n", 7) == -1)
        perror("write");
    if (write(fd2, "4- fd2\n", 7) == -1)
        perror("write");
    if (write(fd3, "5- fd3\n", 7) == -1)
        perror("write");


    /* print file offset values */
    fprintf(stdout, "\ncurrent file offset values:\nfd1 = %ld, fd2 = %ld, fd3 = %ld\n", 
            lseek(fd1, 0, SEEK_CUR), 
            lseek(fd2, 0, SEEK_CUR), 
            lseek(fd3, 0, SEEK_CUR));

    /* change file offset values */
    fprintf(stdout, "\nchanging fd1 offset value to 0\n");
    lseek(fd1, 0, SEEK_SET);

    /* print file offset values */
    fprintf(stdout, "\ncurrent file offset values:\nfd1 = %ld, fd2 = %ld, fd3 = %ld\n", 
            lseek(fd1, 0, SEEK_CUR), 
            lseek(fd2, 0, SEEK_CUR), 
            lseek(fd3, 0, SEEK_CUR));

    /* check special cases */

    /* case 1: bad file descriptor */
    /* find unused file descriptor */
    for (i = 0; i < 100; ++i)
    {
        if (fcntl(i, F_GETFL) == -1)
        {
            fprintf(stdout, "\ngiving unused file descriptor to custom_dup2 as old file descriptor\n");
            errno = 0;
            invalid_fd = i;
            custom_dup2(i, fd3);
            perror("custom_dup2");
            break;
        }
    }

    /* case 2: provide already used file descriptor */
    fd4 = open(filename2, O_WRONLY | O_CREAT, 0666);
    if (fd4 == -1)
        error_exit("open");

    fprintf(stdout, "\n%s file is opened with fd4 = %d\n", filename2, fd4);

    /* write file */
    fprintf(stdout, "\nwriting to file %s with fd4\n", filename2);
    if (write(fd4, "6- fd4\n", 7) == -1)
        perror("write");

    /* duplicate file descriptor */
    fprintf(stdout, "\nduplicating fd4 to fd3\n");
    fd3 = custom_dup2(fd4, fd3);

    fprintf(stdout, "\ncurrent file descriptor values:\nfd1 = %d, fd2 = %d, fd3 = %d, fd4 = %d\n", 
            fd1, fd2, fd3, fd4);

    /* write file */
    fprintf(stdout, "\nwriting to file fd3 and fd4\n");
    if (write(fd3, "7- fd3\n", 7) == -1)
        perror("write");
    if (write(fd4, "8- fd4\n", 7) == -1)
        perror("write");

    /* case 3: use same file descriptor for both old and new */
    fprintf(stdout, "\nduplicating fd4 to fd4\n");
    errno = 0;
    fd4 = custom_dup2(fd4, fd4);
    perror("custom_dup2");

    fprintf(stdout, "\nduplicating invalid file descriptor to invalid file descriptor\n");
    
    for (i = 0; i < 100; ++i)
    {
        if (fcntl(i, F_GETFL) == -1)
        {
            invalid_fd = i;
            break;
        }
    }
    errno = 0;
    invalid_fd = custom_dup2(invalid_fd, invalid_fd);
    perror("custom_dup2");

    fprintf(stdout, "\nclosing all file descriptors\n");

    /* close files */
    if (close(fd1) == -1)
        error_exit("close");
    if (close(fd2) == -1)
        error_exit("close");
    if (close(fd3) == -1)
        error_exit("close");
    if (close(fd4) == -1)
        error_exit("close");

    return 0;
}
