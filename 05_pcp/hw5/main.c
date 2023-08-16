#include "include/mycommon.h"
#include "include/thread_pool.h"

sig_atomic_t signal_occurred = 0;
pcp_stats_t pcp_stats;
long long open_fd_limit = 250;

/* signal handler */
void signal_handler (int signum)
{
    signal_occurred = signum;
}

int main (int argc, char *argv[])
{
    size_t buffer_size, number_of_consumers;
    char source_dir[MAX_FILENAME_LEN];
    char destination_dir[MAX_FILENAME_LEN];
    char buffer[MAX_FILENAME_LEN];
    struct sigaction sa;
    producer_args_t producer_args;
    consumer_args_t consumer_args;
    struct timeval start_time, end_time;
    long long elapsed_time;
    DIR *dir;
    struct rlimit rlim;

    /* get the maximum number of open file descriptors */
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
        error_exit("getrlimit");

    /* print the maximum number of open file descriptors */
    open_fd_limit = rlim.rlim_cur;
    printf("Maximum number of open file descriptors: %lld\n", open_fd_limit);

    /* initialize statistics */
    pcp_stats.num_open_file_desc = 0;
    pcp_stats.num_reg_files = 0;
    pcp_stats.num_dir_files = 0;
    pcp_stats.num_fifo_files = 0;
    pcp_stats.total_bytes = 0;


    /* set signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    
    setbuf(stdout, NULL);

    /* command line = ./main <buffer size> <number of consumers> <source directory> <destination directory> */

    if (argc != 5)
        error_exit_custom("Usage: ./main <buffer size> <number of consumers> <source directory> <destination directory>");

    buffer_size = string_to_uint(argv[1], strlen(argv[1]));
    number_of_consumers = string_to_uint(argv[2], strlen(argv[2]));

    if (buffer_size == 0 || number_of_consumers == 0)
        error_exit_custom("Usage: ./main <buffer size> <number of consumers> <source directory> <destination directory>");

    block_all_signals();

    snprintf(source_dir, MAX_FILENAME_LEN, "%s", argv[3]);
    snprintf(destination_dir, MAX_FILENAME_LEN, "%s", argv[4]);

    /* check if paths are given with " characters */
    if (source_dir[0] == '"' && source_dir[strlen(source_dir) - 1] == '"')
    {
        source_dir[strlen(source_dir) - 1] = '\0';
        snprintf(buffer, MAX_FILENAME_LEN, "%s", source_dir + 1);
        snprintf(source_dir, MAX_FILENAME_LEN, "%s", buffer);
    }

    if (destination_dir[0] == '"' && destination_dir[strlen(destination_dir) - 1] == '"')
    {
        destination_dir[strlen(destination_dir) - 1] = '\0';
        snprintf(buffer, MAX_FILENAME_LEN, "%s", destination_dir + 1);
        snprintf(destination_dir, MAX_FILENAME_LEN, "%s", buffer);
    }

    /* check if source directory exists */
    if ((dir = opendir(source_dir)) == NULL)
        error_exit_custom("Source directory does not exist");
    
    closedir(dir);

    /* print the arguments */
    printf("Buffer size: %zu\n", buffer_size);
    printf("Number of consumers: %zu\n", number_of_consumers);
    printf("Source directory: %s\n", source_dir);
    printf("Destination directory: %s\n", destination_dir);

    /* initialize thread pool arguments */
    snprintf(producer_args.source_dir, MAX_FILENAME_LEN, "%s", source_dir);
    snprintf(producer_args.dest_dir, MAX_FILENAME_LEN, "%s", destination_dir);
    snprintf(consumer_args.dest_dir, MAX_FILENAME_LEN, "%s", destination_dir);
    producer_args.buffer_size = buffer_size;

    unblock_all_signals();

    /* start the timer */
    gettimeofday(&start_time, NULL);

    /* create the thread pool */
    if (thread_pool_init(&number_of_consumers, &producer_args, &consumer_args) == -1)
    {
        error_exit_custom("thread_pool_init");
    }

    /* destroy the thread pool */
    if (thread_pool_destroy() == -1)
        error_exit_custom("thread_pool_destroy");

    /* stop the timer */
    gettimeofday(&end_time, NULL);


    /* calculate elapsed time in microseconds */
    elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000;
    elapsed_time += end_time.tv_usec - start_time.tv_usec;

    /* print statistics */
    fprintf(stdout, "\nPCP statistics:\n");
    fprintf(stdout, "Number of regular files copied: %zu\n", pcp_stats.num_reg_files);
    fprintf(stdout, "Number of directories copied: %zu\n", pcp_stats.num_dir_files);
    fprintf(stdout, "Number of FIFOs copied: %zu\n", pcp_stats.num_fifo_files);
    fprintf(stdout, "Total number of bytes copied: %zu\n", pcp_stats.total_bytes);
    fprintf(stdout, "Elapsed time: %lld microseconds\n", elapsed_time);
    /* fprintf(stdout, "Number of open file descriptors: %ld\n", pcp_stats.num_open_file_desc); */
    fprintf(stdout, "Terminating...\n");    

    return 0;
}