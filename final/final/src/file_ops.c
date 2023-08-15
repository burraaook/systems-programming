#include "../include/file_ops.h"

char *ignore_file = "bibakBOXClient_LOG.BIBAKLOG";

int destroy_file_info_list_prev (file_info_list_t **prev_file_info_list_head, file_info_list_t **prev_file_info_list_tail)
{
    file_info_list_t *temp;

    while (*prev_file_info_list_head != NULL)
    {
        temp = *prev_file_info_list_head;
        *prev_file_info_list_head = (*prev_file_info_list_head)->next;
        free(temp);
    }

    *prev_file_info_list_head = NULL;
    *prev_file_info_list_tail = NULL;
    return 0;
}


int destroy_file_info_list_current (file_info_list_t **current_file_info_list_head, file_info_list_t **current_file_info_list_tail)
{
    file_info_list_t *temp;

    while (*current_file_info_list_head != NULL)
    {
        temp = *current_file_info_list_head;
        *current_file_info_list_head = (*current_file_info_list_head)->next;
        free(temp);
    }

    *current_file_info_list_head = NULL;
    *current_file_info_list_tail = NULL;
    return 0;
}

/* put the current file info list into the previous file info list */
int switch_to_prev (file_info_list_t **prev_file_info_list_head, file_info_list_t **prev_file_info_list_tail, 
                    file_info_list_t **current_file_info_list_head, file_info_list_t **current_file_info_list_tail)
{
    *prev_file_info_list_head = *current_file_info_list_head;
    *prev_file_info_list_tail = *current_file_info_list_tail;
    *current_file_info_list_head = NULL;
    *current_file_info_list_tail = NULL;
    return 0;
}

/* traverse the directory, and fill the file_info_list */
int fill_file_info_list (const char *dir_name, file_info_list_t **file_info_list_head, file_info_list_t **file_info_list_tail, const char *source_dir)
{
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char path[MAX_PATH_LEN];

    if ((dir = opendir(dir_name)) == NULL)
    {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* if it is ignore file, ignore it */
        if (strcmp(entry->d_name, ignore_file) == 0)
            continue;

        snprintf(path, MAX_PATH_LEN, "%s/%s", dir_name, entry->d_name);

        if (lstat(path, &file_stat) == -1)
        {
            /* file is deleted from outside */
            continue;
        }

        if (S_ISDIR(file_stat.st_mode))
        {
            if (fill_file_info_list(path, file_info_list_head, file_info_list_tail, source_dir) == -1)
            {
                closedir(dir);
                /* directory is deleted from outside */
                return 0;
            }
        
            /* add to the list as a directory */
            if (add_file_info_list(path, &file_stat, DIRECTORY_TYPE, file_info_list_head, file_info_list_tail, source_dir) == -1)
            {
                closedir(dir);
                return -1;
            }        
        }
        else if (S_ISREG(file_stat.st_mode))
        {
            if (add_file_info_list(path, &file_stat, REGULAR_FILE_TYPE, file_info_list_head, file_info_list_tail, source_dir) == -1)
            {
                closedir(dir);
                return -1;
            }
        }

        /* fifo */
        else if (S_ISFIFO(file_stat.st_mode))
        {
            if (add_file_info_list(path, &file_stat, FIFO_FILE_TYPE, file_info_list_head, file_info_list_tail, source_dir) == -1)
            {
                closedir(dir);
                return -1;
            }
        }

        else
        {
            fprintf(stderr, "Unknown file type\n");
        }
    }

    closedir(dir);
    return 0;
}

int add_file_info_list (const char *path, struct stat *file_stat, file_type_t file_type, 
                        file_info_list_t **file_info_list_head, file_info_list_t **file_info_list_tail,
                        const char *source_dir)
{
    char temp_path[MAX_PATH_LEN];
    file_info_list_t *new_node;

    if ((new_node = malloc(sizeof(file_info_list_t))) == NULL)
    {
        error_print("malloc");
        return -1;
    }
    /* ignore the source directory */
    snprintf(temp_path, MAX_PATH_LEN, "%s", path + strlen(source_dir) + 1);

    snprintf(new_node->file_info.path, MAX_PATH_LEN, "%s", temp_path);
    new_node->file_info.file_size = file_stat->st_size;
    new_node->file_info.last_modified = file_stat->st_mtime;
    new_node->file_info.file_type = file_type;
    new_node->file_info.updateFlag = 0;
    new_node->next = NULL;

    if (*file_info_list_head == NULL)
    {
        *file_info_list_head = new_node;
        *file_info_list_tail = new_node;
    }
    else
    {
        (*file_info_list_tail)->next = new_node;
        *file_info_list_tail = new_node;
    }

    return 0;
}

/* compare the previous file info list with the current file info list */
/* return 0 if they are the same, 1 if they are different */
/* it is a linked list, and add using allocating memory */
/* add to the list if file is added, deleted, or modified */
int compare_lists (comp_result_list_t **comp_result_list, file_info_list_t *prev_file_info_list_head, file_info_list_t *current_file_info_list_head)
{
    file_info_list_t *current_node;
    file_info_list_t *prev_node;
    comp_result_t comp_result;
    comp_result_list_t *comp_result_list_head = NULL;
    comp_result_list_t *comp_result_list_tail = NULL;
    int found;
    int change_flag = 0;

    found = 0;
    current_node = current_file_info_list_head;
    prev_node = prev_file_info_list_head;

    /* check for added files and modified files */
    /* start with current list */
    while (current_node != NULL)
    {
        prev_node = prev_file_info_list_head;
        
        /* check if it is in the previous list */
        while (prev_node != NULL)
        {
            /* if it is in the previous list */
            if (strcmp(current_node->file_info.path, prev_node->file_info.path) == 0)
            {
                found = 1;

                /* check if it is modified */
                if (current_node->file_info.last_modified != prev_node->file_info.last_modified)
                {
                    /* if it is a directory, do not add to the list */
                    if (current_node->file_info.file_type == DIRECTORY_TYPE)
                        break;

                    /* add to the list */
                    comp_result.comp_result = FILE_MODIFIED;
                    comp_result.file_info = current_node->file_info;
                    comp_result_list_add(&comp_result_list_head, &comp_result_list_tail, &comp_result);
                    change_flag = 1;
                }

                break;
            }

            prev_node = prev_node->next;
        }

        /* if it is not in the previous list */
        if (found == 0)
        {
            /* add to the list */
            comp_result.comp_result = FILE_ADDED;
            comp_result.file_info = current_node->file_info;
            comp_result_list_add(&comp_result_list_head, &comp_result_list_tail, &comp_result);
            change_flag = 1;
        }

        found = 0;
        current_node = current_node->next;
    }

    /* check for deleted files */
    /* start with previous list */
    prev_node = prev_file_info_list_head;

    while (prev_node != NULL)
    {
        current_node = current_file_info_list_head;
        found = 0;

        /* check if it is in the current list */
        while (current_node != NULL)
        {
            /* if it is in the current list */
            if (strcmp(prev_node->file_info.path, current_node->file_info.path) == 0)
            {
                found = 1;
                break;
            }

            current_node = current_node->next;
        }

        /* if it is not in the current list */
        if (found == 0)
        {
            /* add to the list */
            comp_result.comp_result = FILE_DELETED;
            comp_result.file_info = prev_node->file_info;
            comp_result_list_add(&comp_result_list_head, &comp_result_list_tail, &comp_result);
            change_flag = 1;
        }

        prev_node = prev_node->next;
    }

    /* assign comp_result_list */
    *comp_result_list = comp_result_list_head;
    return change_flag;
}

void comp_result_list_add (comp_result_list_t **comp_result_list_head, comp_result_list_t **comp_result_list_tail, const comp_result_t *comp_result)
{
    comp_result_list_t *new_node;

    if ((new_node = malloc(sizeof(comp_result_list_t))) == NULL)
    {
        error_print("malloc");
        return;
    }

    new_node->comp_result = *comp_result;
    new_node->next = NULL;

    if (*comp_result_list_head == NULL)
    {
        *comp_result_list_head = new_node;
        *comp_result_list_tail = new_node;
    }
    else
    {
        (*comp_result_list_tail)->next = new_node;
        *comp_result_list_tail = new_node;
    }
}

void print_current_list (file_info_list_t *current_file_info_list_head)
{
    file_info_list_t *current_node;

    current_node = current_file_info_list_head;

    while (current_node != NULL)
    {
        printf("\npath: %s\n", current_node->file_info.path);
        printf("file size: %ld\n", current_node->file_info.file_size);
        printf("last modified: %ld\n", current_node->file_info.last_modified);
        printf("file type: %d\n", current_node->file_info.file_type);

        current_node = current_node->next;
    }
}

void print_prev_list (file_info_list_t *prev_file_info_list_head)
{
    file_info_list_t *prev_node;

    prev_node = prev_file_info_list_head;

    while (prev_node != NULL)
    {
        printf("\npath: %s\n", prev_node->file_info.path);
        printf("file size: %ld\n", prev_node->file_info.file_size);
        printf("last modified: %ld\n", prev_node->file_info.last_modified);
        printf("file type: %d\n", prev_node->file_info.file_type);

        prev_node = prev_node->next;
    }
}

void print_comp_result_list (comp_result_list_t *comp_result_list)
{
    comp_result_list_t *current_node;

    current_node = comp_result_list;

    while (current_node != NULL)
    {
        printf("\ncomp_result: %d\n", current_node->comp_result.comp_result);
        printf("path: %s\n", current_node->comp_result.file_info.path);
        printf("file size: %ld\n", current_node->comp_result.file_info.file_size);
        printf("last modified: %ld\n", current_node->comp_result.file_info.last_modified);
        printf("file type: %d\n", current_node->comp_result.file_info.file_type);
        printf("updateFlag: %d\n", current_node->comp_result.file_info.updateFlag);
        printf("\n");

        current_node = current_node->next;
    }
}

int comp_result_to_buffer (const comp_result_t *comp_result, char *buffer)
{
    /* make string of comp_result */

    if (comp_result->comp_result == FILE_ADDED)
    {
        snprintf(buffer, MAX_BUF_LEN, 
        "COMMAND: FILE_ADDED, PATH: %s, FILE_SIZE: %ld, LAST_MODIFIED: %ld, FILE_TYPE: %d, UPDATE_FLAG: %d, LAST_MODIFIED_CLIENT: %ld",
        comp_result->file_info.path, comp_result->file_info.file_size, comp_result->file_info.last_modified, comp_result->file_info.file_type,
        comp_result->file_info.updateFlag, comp_result->file_info.last_modified_client);

        return 0;
    }

    else if (comp_result->comp_result == FILE_DELETED)
    {
        snprintf(buffer, MAX_BUF_LEN, 
        "COMMAND: FILE_DELETED, PATH: %s, FILE_SIZE: %ld, LAST_MODIFIED: %ld, FILE_TYPE: %d, UPDATE_FLAG: %d, LAST_MODIFIED_CLIENT: %ld",
        comp_result->file_info.path, comp_result->file_info.file_size, comp_result->file_info.last_modified, comp_result->file_info.file_type,
        comp_result->file_info.updateFlag, comp_result->file_info.last_modified_client);

        return 0;
    }

    else if (comp_result->comp_result == FILE_MODIFIED)
    {
        snprintf(buffer, MAX_BUF_LEN, 
        "COMMAND: FILE_MODIFIED, PATH: %s, FILE_SIZE: %ld, LAST_MODIFIED: %ld, FILE_TYPE: %d, UPDATE_FLAG: %d, LAST_MODIFIED_CLIENT: %ld",
        comp_result->file_info.path, comp_result->file_info.file_size, comp_result->file_info.last_modified, comp_result->file_info.file_type,
        comp_result->file_info.updateFlag, comp_result->file_info.last_modified_client);

        return 0;
    }

    else 
    {
        fprintf(stderr, "Unknown comp_result\n");
        return -1;
    }

    return 0;
}

int buffer_to_comp_result (const char *buffer, comp_result_t *comp_result)
{
    /* convert buffer to comp_result */
    char temp_buffer[MAX_BUF_LEN];
    char *token;
    char *saveptr = NULL;
    char *command;
    char *path;
    char *file_size;
    char *last_modified;
    char *file_type;
    char *updateFlag;
    char *last_modified_client;

    snprintf(temp_buffer, MAX_BUF_LEN, "%s", buffer);
    /* get command */
    token = strtok_r(temp_buffer, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    command = token + strlen("COMMAND: ");

    /* get path */
    token = strtok_r(NULL, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    path = token + strlen(" PATH: ");

    /* get file_size */
    token = strtok_r(NULL, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    file_size = token + strlen(" FILE_SIZE: ");

    /* get last_modified */
    token = strtok_r(NULL, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    last_modified = token + strlen(" LAST_MODIFIED: ");

    /* get file_type */
    token = strtok_r(NULL, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    file_type = token + strlen(" FILE_TYPE: ");

    /* get updateFlag */
    token = strtok_r(NULL, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    updateFlag = token + strlen(" UPDATE_FLAG: ");

    /* get last_modified_client */
    token = strtok_r(NULL, ",", &saveptr);
    if (token == NULL)
    {
        fprintf(stderr, "token is NULL\n");
        return -1;
    }
    last_modified_client = token + strlen(" LAST_MODIFIED_CLIENT: ");

    /* assign comp_result */
    snprintf(comp_result->file_info.path, MAX_PATH_LEN, "%s", path);
    comp_result->file_info.file_size = string_to_uint(file_size, strlen(file_size));
    comp_result->file_info.last_modified = string_to_uint(last_modified, strlen(last_modified));
    comp_result->file_info.file_type = string_to_uint(file_type, strlen(file_type));
    comp_result->file_info.updateFlag = string_to_uint(updateFlag, strlen(updateFlag));
    comp_result->file_info.last_modified_client = string_to_uint(last_modified_client, strlen(last_modified_client));

    if (strcmp(command, "FILE_ADDED") == 0)
    {
        comp_result->comp_result = FILE_ADDED;
        return 0;
    }

    else if (strcmp(command, "FILE_DELETED") == 0)
    {
        comp_result->comp_result = FILE_DELETED;
        return 0;
    }

    else if (strcmp(command, "FILE_MODIFIED") == 0)
    {
        comp_result->comp_result = FILE_MODIFIED;
        return 0;
    }

    else
    {
        fprintf(stderr, "Unknown command\n");
        return -1;
    }

    return 0;
}

int is_current_old (const comp_result_t *comp_result, const char *dir_name)
{
    char path[MAX_PATH_LEN+2];
    struct stat file_stat;

    /* add dir_name to beginning of path */
    snprintf(path, MAX_PATH_LEN+2, "%s/%s", dir_name, comp_result->file_info.path);

    /* check if it is deleted */
    if (comp_result->comp_result == FILE_DELETED)
    {
        /* file is deleted */
        return 1;
    }

    /* check if last modified of given file is older than file in current directory */
    if (lstat(path, &file_stat) == -1)
    {
        /* file does not exist */
        return 1;
    }

    /* return 1 if current is old, 0 if current is new */
    if (comp_result->file_info.last_modified > file_stat.st_mtime)
        return 1;
    
    return 0;
}

/* free the linked list */
void free_comp_result_list (comp_result_list_t **comp_result_list)
{
    comp_result_list_t *temp;

    while (*comp_result_list != NULL)
    {
        temp = *comp_result_list;
        *comp_result_list = (*comp_result_list)->next;
        free(temp);
    }

    *comp_result_list = NULL;
}

int checking_func (file_info_list_t **head_prev, file_info_list_t **tail_prev,
                   file_info_list_t **head_cur, file_info_list_t **tail_cur,
                   comp_result_list_t **comp_result_list_head, 
                   int *change_flag, const char *dir_name)
{

    /* destroy comp_result_list */
    free_comp_result_list(comp_result_list_head);

    /* destroy previous file_info_list */
    if (destroy_file_info_list_prev(head_prev, tail_prev) == -1)
    {
        error_print_custom("destroy_file_info_list_prev");
        return -1;
    }

    /* initialize prev to NULL */
    *head_prev = NULL;
    *tail_prev = NULL;

    /* switch to prev */
    if (switch_to_prev(head_prev, tail_prev, head_cur, tail_cur) == -1)
    {
        error_print_custom("switch_to_prev");
        return -1;
    }

    /* initialize current to NULL */
    *head_cur = NULL;
    *tail_cur = NULL;

    /* fill file info list */
    if (fill_file_info_list(dir_name, head_cur, tail_cur, dir_name) == -1)
    {
        error_print_custom("fill_file_info_list");
        return -1;
    }

    /* compare lists */
    *change_flag = compare_lists(comp_result_list_head, *head_prev, *head_cur);

    if (*change_flag == -1)
        return -1;

    return 0;
}

int open_file_and_create_dirs (const comp_result_t *comp_result, const char *dir_name, int *file_fd,
                                 file_info_list_t **list_head, file_info_list_t **list_tail)
{
    char path[MAX_PATH_LEN+2];
    int file_exists = 0;

    /* add dir_name to beginning of path */
    snprintf(path, MAX_PATH_LEN+2, "%s/%s", dir_name, comp_result->file_info.path);

    /* check if file exists on the path */
    file_exists = access(path, F_OK);

    /* if file exists */
    if (file_exists == 0)
    {
        /* open it and truncate it */
        *file_fd = open(path, O_WRONLY | O_TRUNC);

        if (*file_fd == -1)
        {
            error_print("open1");
            return -1;
        }

        return 0;
    }
    else 
    {
        if (create_dirs(path, list_head, list_tail, dir_name) == -1)
        {
            error_print("create_dirs");
            return -1;
        }

        /* open it and truncate it */
        *file_fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if (*file_fd == -1)
        {
            error_print("open2");
            return -1;
        }
    }   

    return 0;
}

/* create all directories at given hierarchy in the path */
/* do not create the file at the end as a directory or a file */
int create_dirs (const char *path, file_info_list_t **list_head, file_info_list_t **list_tail, const char *dir_name)
{
    char temp_path[MAX_PATH_LEN+2];
    char base_path[MAX_PATH_LEN+4];
    char temp_path2[MAX_PATH_LEN+2];
    char *dir = NULL;
    char *saveptr = NULL;
    comp_result_t comp_result;

    memset(&comp_result, 0, sizeof(comp_result_t));
    memset(temp_path, 0, MAX_PATH_LEN+2);
    memset(base_path, 0, MAX_PATH_LEN+4);
    memset(temp_path2, 0, MAX_PATH_LEN+2);

    snprintf(temp_path, MAX_PATH_LEN+2, "%s", path + strlen(dir_name) + 1);
    /* get the first directory */
    dir = strtok_r(temp_path, "/", &saveptr);

    if (dir == NULL) {
        /* No directories in the path */
        return 0;
    }

    snprintf(base_path, MAX_PATH_LEN, "%s", dir_name);
    snprintf(temp_path2, MAX_PATH_LEN+4, "%s", base_path);
    snprintf(base_path, MAX_PATH_LEN+4, "%s/%s", temp_path2, dir);

    while ((dir = strtok_r(NULL, "/", &saveptr)) != NULL)
    {
        /* check if directory exists */
        if (access(base_path, F_OK) != 0)
        {
            /* create directory */
            if (mkdir(base_path, 0777) == -1)
            {
                error_print("mkdir");
                return -1;
            }
            else
            {
                /* update file_info_list */
                comp_result.comp_result = FILE_ADDED;
                snprintf(comp_result.file_info.path, MAX_PATH_LEN, "%s", base_path + strlen(dir_name) + 1);
                update_file_info_list(&comp_result, list_head, list_tail, dir_name);
            }
        }
        
        /* update base_path */
        snprintf(temp_path2, MAX_PATH_LEN+4, "%s", base_path);
        snprintf(base_path, MAX_PATH_LEN+4, "%s/%s", temp_path2, dir);
    }

    return 0;
}


int update_file_info_list (const comp_result_t *comp_result, file_info_list_t **file_info_list_head, 
                           file_info_list_t **file_info_list_tail, const char *dir_name)
{
    /* find the given file in the file_info_list, and update its updateFlag, and last_modified_client */

    file_info_list_t *current_node;
    file_info_list_t *prev_node;
    struct stat file_stat;
    char buffer[MAX_BUF_LEN];

    current_node = *file_info_list_head;
    prev_node = NULL;

    if (comp_result == NULL)
    {
        fprintf(stderr, "comp_result is NULL\n");
        return -1;
    }

    if (comp_result->comp_result != FILE_DELETED)
    {

        /* if current list is empty, add it */
        if (current_node == NULL)
        {
            /* add to the list */
            *file_info_list_head = malloc(sizeof(file_info_list_t));

            if (*file_info_list_head == NULL)
            {
                error_print("malloc");
                return -1;
            }

            snprintf((*file_info_list_head)->file_info.path, MAX_PATH_LEN, "%s", comp_result->file_info.path);
            snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result->file_info.path);
            /* update last_modified */
            if (lstat(buffer, &file_stat) == -1)
            {
                error_print("lstat - update_file_info_list");
                return -1;
            }
            
            /* if file size is 0, it might be still processing by file system, so wait a little bit, then try one more time */
            if (file_stat.st_size == 0)
            {
                sleep(1);
                if (lstat(buffer, &file_stat) == -1)
                {
                    error_print("lstat - update_file_info_list");
                    return -1;
                }
            }

            (*file_info_list_head)->file_info.last_modified = file_stat.st_mtime;
            (*file_info_list_head)->file_info.file_size = file_stat.st_size;
            (*file_info_list_head)->file_info.file_type = comp_result->file_info.file_type;
            (*file_info_list_head)->next = NULL;
            *file_info_list_tail = *file_info_list_head;

            /* print file_info_list */
            return 0;
        }

        /* find the given file in the file_info_list */
        while (current_node != NULL)
        {
            if (strcmp(current_node->file_info.path, comp_result->file_info.path) == 0)
            {
                memset(buffer, 0, MAX_BUF_LEN);

                snprintf(buffer, MAX_BUF_LEN, "%s", comp_result->file_info.path);
                snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result->file_info.path);
                
                /* update last_modified */
                if (lstat(buffer, &file_stat) == -1)
                {
                    error_print("lstat - update_file_info_list2");
                    return -1;
                }

                /* if file size is 0, it might be still processing by file system, so wait a little bit, then try one more time */
                if (file_stat.st_size == 0)
                {
                    sleep(1);
                    if (lstat(buffer, &file_stat) == -1)
                    {
                        error_print("lstat - update_file_info_list2");
                        return -1;
                    }
                }

                current_node->file_info.last_modified = file_stat.st_mtime;
                current_node->file_info.file_size = file_stat.st_size;

                /* print file_info_list */
                return 0;
            }

            prev_node = current_node;
            current_node = current_node->next;
        }

        /* if it is not in the list, add it */
        if (prev_node != NULL)
        {

            snprintf(buffer, MAX_BUF_LEN, "%s/%s", dir_name, comp_result->file_info.path);
            /* update last_modified */
            if (lstat(buffer, &file_stat) == -1)
            {
                error_print("lstat - update_file_info_list3");
                return -1;
            }

            /* if file size is 0, it might be still processing by file system, so wait a little bit, then try one more time */
            if (file_stat.st_size == 0)
            {
                sleep(1);
                if (lstat(buffer, &file_stat) == -1)
                {
                    error_print("lstat - update_file_info_list3");
                    return -1;
                }
            }

            prev_node->next = malloc(sizeof(file_info_list_t));

            if (prev_node->next == NULL)
            {
                error_print("malloc");
                return -1;
            }

            snprintf(prev_node->next->file_info.path, MAX_PATH_LEN, "%s", comp_result->file_info.path);
            prev_node->next->file_info.last_modified = file_stat.st_mtime;
            prev_node->next->file_info.file_size = file_stat.st_size;
            prev_node->next->file_info.file_type = comp_result->file_info.file_type;
            prev_node->next->next = NULL;
            *file_info_list_tail = prev_node->next;
        }
    }
    else
    {
        /* delete the given file from the file_info_list */
        prev_node = NULL;
        current_node = *file_info_list_head;

        while (current_node != NULL)
        {
            if (strcmp(current_node->file_info.path, comp_result->file_info.path) == 0)
            {
                if (prev_node == NULL)
                {
                    /* node to be deleted is the head */
                    *file_info_list_head = current_node->next;
                    free(current_node);

                    if (*file_info_list_head == NULL)
                        *file_info_list_tail = NULL;

                    /* print file_info_list */          
                    return 0;
                }
                else
                {
                    prev_node->next = current_node->next;
                    free(current_node);

                    if (prev_node->next == NULL)
                        *file_info_list_tail = prev_node;

                    return 0;
                }
            }

            prev_node = current_node;
            current_node = current_node->next;
        }
    }

    return 0;
}

/* delete all files and directories including the given directory */
int delete_all_files_and_dir (const char *dir_name, file_info_list_t **list_head, file_info_list_t **list_tail,
                              const char *source_dir)
{
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char path[MAX_PATH_LEN-1];
    comp_result_t temp_comp_result;
    memset(&temp_comp_result, 0, sizeof(comp_result_t));

    if ((dir = opendir(dir_name)) == NULL)
    {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, MAX_PATH_LEN-1, "%s/%s", dir_name, entry->d_name);

        if (lstat(path, &file_stat) == -1)
        {
            /* file is deleted from outside */
            continue;
        }

        if (S_ISDIR(file_stat.st_mode))
        {
            /* delete all files and directories in the directory */
            if (delete_all_files_and_dir(path, list_head, list_tail, source_dir) == -1)
            {
                closedir(dir);
                return -1;
            }

            /* update file_info_list */
            snprintf(temp_comp_result.file_info.path, MAX_PATH_LEN, "%s/%s", source_dir, path);
            temp_comp_result.comp_result = FILE_DELETED;
            temp_comp_result.file_info.file_type = DIRECTORY_TYPE;
            update_file_info_list(&temp_comp_result, list_head, list_tail, source_dir);
        }
        else if (S_ISREG(file_stat.st_mode))
        {
            /* delete the file */
            if (unlink(path) == -1)
            {
                error_print("unlink");
                closedir(dir);
                return -1;
            }

            /* update file_info_list */
            snprintf(temp_comp_result.file_info.path, MAX_PATH_LEN, "%s/%s", source_dir, path);
            temp_comp_result.comp_result = FILE_DELETED;
            temp_comp_result.file_info.file_type = REGULAR_FILE_TYPE;
            update_file_info_list(&temp_comp_result, list_head, list_tail, source_dir);
        }

        /* fifo */
        else if (S_ISFIFO(file_stat.st_mode))
        {
            /* delete the fifo */
            if (unlink(path) == -1)
            {
                error_print("unlink");
                closedir(dir);
                return -1;
            }

            /* update file_info_list */
            snprintf(temp_comp_result.file_info.path, MAX_PATH_LEN, "%s/%s", source_dir, path);
            temp_comp_result.comp_result = FILE_DELETED;
            temp_comp_result.file_info.file_type = FIFO_FILE_TYPE;
            update_file_info_list(&temp_comp_result, list_head, list_tail, source_dir);
        }

        else
        {
            fprintf(stderr, "Unknown file type\n");
        }
    }

    /* delete the directory */
    if (rmdir(dir_name) == -1)
    {
        error_print("rmdir");
        closedir(dir);
        return -1;
    }

    /* update file_info_list */
    snprintf(temp_comp_result.file_info.path, MAX_PATH_LEN, "%s", dir_name + strlen(source_dir) + 1);
    temp_comp_result.comp_result = FILE_DELETED;
    temp_comp_result.file_info.file_type = DIRECTORY_TYPE;
    update_file_info_list(&temp_comp_result, list_head, list_tail, source_dir);

    closedir(dir);
    return 0;
}
