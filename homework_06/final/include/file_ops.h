#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "mycommon.h"

typedef enum file_type_t
{
    REGULAR_FILE_TYPE = 0,
    DIRECTORY_TYPE = 1,
    FIFO_FILE_TYPE = 2
} file_type_t;

typedef enum comp_result_enum_t
{
    FILE_ADDED = 0,
    FILE_DELETED = 1,
    FILE_MODIFIED = 2
} comp_result_enum_t;

typedef struct file_info_t
{
    file_type_t file_type;
    char path[MAX_PATH_LEN];
    size_t file_size;
    time_t last_modified;
    int updateFlag;
    time_t last_modified_client;
} file_info_t;

typedef struct file_info_list_t
{
    file_info_t file_info;
    struct file_info_list_t *next;
} file_info_list_t;

typedef struct comp_result_t
{
    file_info_t file_info;
    comp_result_enum_t comp_result;
} comp_result_t;

typedef struct comp_result_list_t
{
    comp_result_t comp_result;
    struct comp_result_list_t *next;
} comp_result_list_t;

int destroy_file_info_list_prev (file_info_list_t **prev_file_info_list_head, file_info_list_t **prev_file_info_list_tail);

int destroy_file_info_list_current (file_info_list_t **current_file_info_list_head, file_info_list_t **current_file_info_list_tail);

int switch_to_prev (file_info_list_t **prev_file_info_list_head, file_info_list_t **prev_file_info_list_tail, 
                    file_info_list_t **current_file_info_list_head, file_info_list_t **current_file_info_list_tail);

int fill_file_info_list (const char *dir_name, file_info_list_t **file_info_list_head, 
                        file_info_list_t **file_info_list_tail, const char *source_dir);

int add_file_info_list (const char *path, struct stat *file_stat, file_type_t file_type, 
                        file_info_list_t **file_info_list_head, file_info_list_t **file_info_list_tail, 
                        const char *source_dir);

int compare_lists (comp_result_list_t **comp_result_list, file_info_list_t *prev_file_info_list_head,
                   file_info_list_t *current_file_info_list_head);

void comp_result_list_add (comp_result_list_t **comp_result_list_head, 
                           comp_result_list_t **comp_result_list_tail, const comp_result_t *comp_result);

void print_current_list (file_info_list_t *current_file_info_list_head);

void print_prev_list (file_info_list_t *prev_file_info_list_head);

void print_comp_result_list (comp_result_list_t *comp_result_list);

int comp_result_to_buffer (const comp_result_t *comp_result, char *buffer);

int buffer_to_comp_result (const char *buffer, comp_result_t *comp_result);

int is_current_old (const comp_result_t *comp_result, const char *dir_name);

void free_comp_result_list (comp_result_list_t **comp_result_list);

int checking_func (file_info_list_t **head_prev, file_info_list_t **tail_prev,
                   file_info_list_t **head_cur, file_info_list_t **tail_cur,
                   comp_result_list_t **comp_result_list_head, 
                   int *change_flag, const char *dir_name);

int open_file_and_create_dirs (const comp_result_t *comp_result, const char *dir_name, int *file_fd,
                                 file_info_list_t **list_head, file_info_list_t **list_tail);

int create_dirs (const char *path, file_info_list_t **list_head, file_info_list_t **list_tail, const char *dir_name);

int update_file_info_list (const comp_result_t *comp_result, file_info_list_t **file_info_list_head, 
                           file_info_list_t **file_info_list_tail, const char *dir_name);

int delete_all_files_and_dir (const char *dir_name, file_info_list_t **list_head, file_info_list_t **list_tail,
                              const char *source_dir);

#endif