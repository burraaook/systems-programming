#ifndef MYCOMMON_H
#define MYCOMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

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

#endif