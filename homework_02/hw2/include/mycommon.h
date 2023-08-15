#ifndef MYCOMMON_H
#define MYCOMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define RETURN_FAILURE -1

/*
 * custom error exit function which prints the error message and exits
 * with EXIT_FAILURE
*/
void error_exit_custom(const char *msg);

/*
 * custom error exit function which prints the error message using perror
 * exits with EXIT_FAILURE
*/
void error_exit(const char *msg);

/*
 * converts string to size_t
 * checks for invalid characters
*/
size_t string_to_uint(const char *str, const size_t len);

/*
 * writes current time stamp to time_stamp
 * format is YYYY-MM-DD-HH:MM:SS
*/
void get_timestamp(char *time_stamp);

/*
 * custom error print function which prints the error message as usage error
*/
void error_print_custom(const char *msg);

/*
 * custom error print function which prints the error message using perror
*/
void error_print(const char *msg);

#endif