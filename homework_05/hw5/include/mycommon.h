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
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/resource.h>

#define RETURN_FAILURE -1
#define MAX_FILENAME_LEN 256
#define MAX_BUF_LEN 256
#define MAX_PATH_LEN 512

enum state_enum
{
    SV_SUCCESS = 0,
    SV_FAILURE = 1,
};

typedef struct pcp_stats_t
{
    size_t num_reg_files;
    size_t num_dir_files;
    size_t num_fifo_files;
    size_t total_bytes;
    long long num_open_file_desc;
} pcp_stats_t;

extern pcp_stats_t pcp_stats;
extern long long open_fd_limit;
extern sig_atomic_t signal_occurred;

/*
 * custom error exit function which prints the error message and exits
 * with EXIT_FAILURE
*/
void error_exit_custom (const char *msg);

/*
 * custom error exit function which prints the error message using perror
 * exits with EXIT_FAILURE
*/
void error_exit (const char *msg);

/*
 * converts string to size_t
 * checks for invalid characters
*/
size_t string_to_uint (const char *str, const size_t len);

/*
 * converts string to pid_t
 * checks for invalid characters
*/
pid_t string_to_pid (const char *str, const size_t len);

/*
 * custom error print function which prints the error message as usage error
*/
void error_print_custom (const char *msg);

/*
 * custom error print function which prints the error message using perror
*/
void error_print (const char *msg);

/*
* converts string to int, only positive numbers
*/
int string_to_int (const char *str, const size_t len);

/*
* blocks all signals
*/
void block_all_signals ( );

/*
* unblocks all signals
*/
void unblock_all_signals ( );

#endif