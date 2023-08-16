
#include "mycommon.h"

#ifndef BIBOCLIENT_H
#define BIBOCLIENT_H

/* client side command implementation helper functions */
void cleanup_and_terminate(request_t request, char *cl_fifo_name);
void set_signal_flag (int flag);
int write_next_word (const char *buffer, char *word);
int write_file_to_shm (const char *file_name, void **addr, int shm_fd);
int read_file_from_shm (const char *file_name, void **addr, int shm_fd, size_t file_size);
int print_message_shm (void **addr, int shm_fd, size_t file_size);
int write_next_2words (const char *buffer, char *word1, int *word2);
int fill_arguments_w (const char *buffer, char *word1, int *word2, char *word3);

int read_lock_file (int fd, struct flock *fl);
int write_lock_file (int fd, struct flock *fl);
int unlock_file (int fd, struct flock *fl);

#endif