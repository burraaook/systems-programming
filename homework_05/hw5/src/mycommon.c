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
