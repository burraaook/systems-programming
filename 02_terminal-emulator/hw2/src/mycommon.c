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
            error_exit_custom("Given byte is not a number");
        
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

