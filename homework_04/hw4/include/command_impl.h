
#ifndef COMMAND_IMPL_H
#define COMMAND_IMPL_H

#include "mycommon.h"

/* command implementation helper functions */
int write_shm_to_file (const char *file_name, const char *shm_name, size_t size);
int write_file_to_shm (int file_fd, const char *shm_name, size_t size);
int write_message_shm (const char *shm_name, const char *message, size_t size);
int get_help_message (char *message, int command);
int get_file_list_shm (const char *shm_name, size_t *size, const char *dir_name);
int write_line_to_shm (int file_fd, const char *shm_name, size_t *size, int line_num);
int write_nth_line (int file_fd, int line_num, const char *message, const char *file_name);
int write_file_to_shm_cur (int file_fd, int shm_fd, void **addr, ssize_t *bytes_written, size_t *addr_size);
int write_shm_to_file_cur (int file_fd, void **addr, ssize_t size, size_t *addr_size);
ssize_t read_line(int file_fd, char *buffer, size_t buffer_size);
int free_shm (void **addr, int shm_fd, size_t size);

#endif