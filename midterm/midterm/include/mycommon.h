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
#include <semaphore.h>
#include <sys/mman.h>

#define RETURN_FAILURE -1
#define MAX_FILENAME_LEN 256
#define MAX_BUF_LEN 256

#define FIFO_NAME "/tmp/sv_fifo_0625"
#define CL_FIFO_NAME "/tmp/cl_fifo_0625"
#define SVC_FIFO_NAME "/tmp/svc_fifo_0625"
#define FIFO_PERM (S_IRUSR | S_IWUSR)

#define SHM_DEFAULT_SIZE 4096   
#define SHM_NAME "cl_shm"

#define MAX_MESSAGE_LEN 256

#define LOG_FILE_NAME "sv_log/log_%d.txt"
#define LOG_SEM_NAME "/log_sem_%d_0625"
#define WRITE_SEM_NAME "/write_sem_%d_0625"


enum commands
{
    CMD_CONNECT = 0,
    CMD_TRYCONNECT = 1,
    CMD_HELP = 2,
    CMD_LIST = 3,
    CMD_READF = 4,
    CMD_WRITET = 5,
    CMD_UPLOAD = 6,
    CMD_DOWNLOAD = 7,
    CMD_QUIT = 8,
    CMD_KILLSERVER = 9,

    /* help commands */
    CMD_HELP_READF = 10,
    CMD_HELP_WRITET = 11,
    CMD_HELP_UPLOAD = 12,
    CMD_HELP_DOWNLOAD = 13,
    CMD_HELP_QUIT = 14,
    CMD_HELP_KILLSERVER = 15,
    CMD_HELP_LIST = 16
};

enum state_enum
{
    SV_SUCCESS = 0,
    SV_FAILURE = 1,
};

typedef struct request_t
{
    pid_t pid;
    int command;
    pid_t sv_pid;
    size_t file_size;
    char file_name[MAX_FILENAME_LEN];
    char message[MAX_BUF_LEN];
    int arg1;
} request_t;

typedef struct response_t
{
    pid_t sv_pid;
    int flag;
    size_t file_size;
} response_t;

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
 * converts string to pid_t
 * checks for invalid characters
*/
pid_t string_to_pid(const char *str, const size_t len);

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