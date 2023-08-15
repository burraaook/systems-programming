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

void get_timestamp(char *timestamp)
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, 20, "%Y-%m-%d-%H_%M_%S", timeinfo);
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

int send_request (const char *fifo_name, request_t request)
{
    int fd;

    /* open the fifo */
    if ((fd = open(fifo_name, O_WRONLY)) == -1)
    {
        if (errno != EINTR) {
            error_print("open");
            return 0;
        }        
        return -1;
    }

    /* send the request */
    if (write(fd, &request, sizeof(struct request_t)) == -1)
    {
        if (errno != EINTR)
            error_print("write");
        
        close(fd);
        return -1;
    }

    /* close the fifo */
    if (close(fd) == -1)
    {
        error_print("close");
        return -1;
    }

    return 0;
}
int get_response (const char *fifo_name, response_t *response)
{
    int fd;

    /* open the fifo */
    if ((fd = open(fifo_name, O_RDONLY)) == -1)
    {
        if (errno != EINTR) {
            error_print("open");
            return 0;
        }        
        return -1;
    }

    /* read the response */
    if (read(fd, response, sizeof(struct request_t)) == -1)
    {
        if (errno != EINTR)
            error_print("read fifo client");
        
        close(fd);
        return -1;
    }

    /* close the fifo */
    if (close(fd) == -1)
    {
        error_print("close");
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
