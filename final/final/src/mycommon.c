#include "../include/mycommon.h"

void error_exit_custom(const char *msg)
{
    fprintf(stderr, "Usage: %s\n", msg);
    exit(EXIT_FAILURE);
}

void error_print_custom(const char *msg)
{
    fprintf(stderr, "Usage: %s\n", msg);
}

void error_print(const char *msg)
{
    perror(msg);
}

void error_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

size_t string_to_uint(const char *str, const size_t len)
{
    size_t result = 0;
    size_t i = 0;
    size_t temp;

    /* convert one digit at a time */
    for (i = 0; i < len; i++)
    {
        /* check if the digit is a number */
        if (str[i] < '0' || str[i] > '9')
            return 0;
        
        /* convert the digit to an integer */
        temp = str[i] - '0';

        /* add the digit to the result */
        result = result * 10 + temp;
    }
    return result;
}

pid_t string_to_pid(const char *str, const size_t len)
{
    pid_t result = 0;
    size_t i = 0;
    size_t temp;

    /* convert one digit at a time */
    for (i = 0; i < len; i++)
    {
        /* check if the digit is a number */
        if (str[i] < '0' || str[i] > '9')
            return 0;
        
        /* convert the digit to an integer */
        temp = str[i] - '0';

        /* add the digit to the result */
        result = result * 10 + temp;
    }
    return result;
}

int string_to_int (const char *str, const size_t len)
{
    int result = 0;
    size_t i = 0;
    int temp;

    /* convert one digit at a time */
    for (i = 0; i < len; ++i)
    {
        /* check if the digit is a number */
        if (str[i] < '0' || str[i] > '9')
            return -1;
        
        /* convert the digit to an integer */
        temp = str[i] - '0';

        /* add the digit to the result */
        result = result * 10 + temp;
    }
    return result;
}

void block_all_signals ( )
{
    sigset_t mask;

    sigfillset(&mask);

    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
        error_print("sigprocmask");
}

void unblock_all_signals ( )
{
    sigset_t mask;

    sigemptyset(&mask);

    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
        error_print("sigprocmask");
}

int read_str_msg (const int sock_fd, char *buf)
{
    size_t read_bytes = 0;
    size_t target_size = MAX_BUF_LEN;
    ssize_t bytes;

    while (read_bytes < target_size)
    {
        bytes = read(sock_fd, buf + read_bytes, target_size - read_bytes);

        if (bytes == -1)
            return -1;
        else if (bytes == 0)
            return 0;
        else
            read_bytes += bytes;
    }

    return 0;
}
int write_str_msg (const int sock_fd, const char *buf)
{
    size_t write_bytes = 0;
    size_t target_size = MAX_BUF_LEN;
    ssize_t bytes;

    while (write_bytes < target_size)
    {
        bytes = write(sock_fd, buf + write_bytes, target_size - write_bytes);

        if (bytes == -1)
            return -1;
        else if (bytes == 0)
            return 0;
        else
            write_bytes += bytes;
    }

    return 0;
}
