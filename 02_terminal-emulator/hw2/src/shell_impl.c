#include "../include/shell_impl.h"

/* flags for cleanup */
int m_tokens_f = 0;
int m_pids_f = 0;
int m_pipe_fds_f = 0;

/* function that executes the command */
int shell_impl (const char *command)
{
    /* return status */
    int status;

    /* array of pids */
    pid_t *pids = NULL;

    /* process id */
    pid_t pid;
    int i,j;

    /* array of pipes */
    int **pipe_fds = NULL;

    /* array of tokens */
    char **tokens = NULL;
    int num_pipes;

    /* child termination flag */
    int term_flag;
    char buffer[MAX_TOKEN_LEN];
    
    /* number of tokens */
    int num_tokens = 0;
    

    /* set flags to 0 for precaution */
    cur_child = 0;
    child_f = 0;

    if (command == NULL) {
        error_print("command is NULL");
        return 0;
    }

    tokens = (char **) malloc(sizeof(char *) * MAX_TOKENS);
    if (tokens == NULL) {
        error_print("malloc - tokens");
        return 0;
    }

    for (i = 0; i < MAX_TOKENS; i++) {
        tokens[i] = (char *) malloc(sizeof(char) * MAX_TOKEN_LEN);
        if (tokens[i] == NULL) {
            error_print("malloc - tokens[i]");
            return 0;
        }
    }
    /* set flag */
    m_tokens_f = 1;

    /* tokenize the command */
    num_tokens = command_tokenizer(command, tokens, '|');

    /* error is printed in command_tokenizer */
    if (num_tokens == -1)
        return 0;

    if (num_tokens == 0) {
        error_print_custom("no tokens");
        cleanup_tokens(tokens, MAX_TOKENS);
        return 0;
    }
    
    /* if there are n tokens, n - 1 pipes are needed */
    num_pipes = num_tokens - 1;

    /* allocate memory for pipes */
    pipe_fds = (int **) malloc(sizeof(int *) * num_pipes);
    if (pipe_fds == NULL) {
        error_print("malloc - pipe_fds");
        cleanup_tokens(tokens, MAX_TOKENS);

        return 0;
    }

    for (i = 0; i < num_pipes; i++)
    {
        pipe_fds[i] = (int *) malloc(sizeof(int) * 2);
        if (pipe_fds[i] == NULL) {
            error_print("malloc - pipe_fds[i]");
            if (m_tokens_f)
                cleanup_tokens(tokens, MAX_TOKENS);

            return 0;
        }
        
        if (pipe(pipe_fds[i]) == -1) {
            error_print("pipe");
            cleanup_tokens(tokens, MAX_TOKENS);

            return 0;
        }
    }

    /* set flag */
    m_pipe_fds_f = 1;

    /* allocate memory for pids */
    pids = (pid_t *) malloc(sizeof(pid_t) * num_tokens);
    if (pids == NULL) {
        error_print("malloc - pids");
        cleanup_tokens(tokens, MAX_TOKENS);
        cleanup_pipes(pipe_fds, num_pipes);

        return 0;
    }

    /* set flag */
    m_pids_f = 1;

    /* create children */
    for (i = 0; i < num_tokens; ++i)
    {
        pid = fork();
        switch(pid)
        {
            case -1:
                /* fork failed */
                error_print("fork");

                /* free memory */
                cleanup_tokens(tokens, MAX_TOKENS);
                cleanup_pipes(pipe_fds, num_pipes);
                cleanup_pids(pids);

                break;
            case 0:
                /* if not the first child */
                if (i != 0)
                {
                    /* redirect stdin to read end of previous pipe */
                    if (dup2(pipe_fds[i - 1][0], STDIN_FILENO) == -1)
                        error_exit("dup2");
                }

                /* if not the last child */
                if (i != num_tokens - 1)
                {
                    /* redirect stdout to write end of next pipe */
                    if (dup2(pipe_fds[i][1], STDOUT_FILENO) == -1)
                        error_exit("dup2");
                }

                /* close all pipe ends */
                for (j = 0; j < num_pipes; j++)
                {
                    if (fcntl(pipe_fds[j][0], F_GETFD) != -1) {
                        if (close(pipe_fds[j][0]) == -1)
                            error_exit("close");
                    }

                    if (fcntl(pipe_fds[j][1], F_GETFD) != -1) {
                        if (close(pipe_fds[j][1]) == -1)
                            error_exit("close");
                    }
                }
                
                memset(buffer, 0, MAX_TOKEN_LEN);
                strcpy(buffer, tokens[i]);

                /* free memory */
                cleanup_tokens(tokens, MAX_TOKENS);
                cleanup_pipes(pipe_fds, num_pipes);
                cleanup_pids(pids);

                /* execute command */
                handle_redirect(buffer);
                break;

            default:
                pids[i] = pid;
                
                child_f = 1;
                cur_child = pid;

                /* close unused pipe ends */
                if (i != 0)
                {
                    if (close(pipe_fds[i - 1][0]) == -1)
                        error_exit("close");
                }

                if (i != num_tokens - 1)
                {
                    if (close(pipe_fds[i][1]) == -1)
                        error_exit("close");
                }

                /* wait for child to finish */
                if (waitpid(pid, &status, 0) == -1) {
                        error_print("waitpid");
                }

                else
                {
                    term_flag = check_term_signal(status, pid);

                    if (term_flag != -1)
                    {
                        /* free memory */
                        cleanup_tokens(tokens, MAX_TOKENS);
                        cleanup_pids(pids);
                        cleanup_pipes(pipe_fds, num_pipes);
                        child_f = 0;
                        cur_child = 0;

                        return term_flag;
                    }
                }
        }
    }

    /* reset flags */
    child_f = 0;
    cur_child = 0;

    /* free memory */
    cleanup_tokens(tokens, MAX_TOKENS);
    cleanup_pids(pids);
    cleanup_pipes(pipe_fds, num_pipes);

    return 0;
}

/*
 * it assumes tokens are already allocated
 * return number of tokens, -1 if error
*/
int command_tokenizer (const char *command, char **tokens, char delim)
{
    int i = 0;
    int cur = 0;
    char token[MAX_TOKEN_LEN];
    int num_tokens = 0;
    int flag;

    /* if delim is found, split the string and save tokens */
    while (command[cur] != '\0' && num_tokens < MAX_TOKENS)
    {
        flag = 0;

        while ((command[cur] != delim) && (command[cur] != '\0') &&
               (i < MAX_TOKEN_LEN))
        {
            if (command[cur] != ' ')
                flag = 1;
            
            /* skip leading spaces */
            if (!flag && command[cur] == ' ')
            {
                cur++;
                continue;
            }
            token[i] = command[cur];
            i++;
            cur++;
        }

        if (i == MAX_TOKEN_LEN) {
            error_print_custom("command too long");
            return -1;
        }

        if (i != 0)
        {
            token[i] = '\0';
            snprintf(tokens[num_tokens], strlen(token) + 1, "%s", token);

            /* remove trailing spaces */
            trim(tokens[num_tokens]);

            num_tokens++;    
            i = 0;
        }

        /* if delimiter is not null, skip it */
        if (command[cur] != '\0')
            cur++;
    }

    if (num_tokens == MAX_TOKENS && command[cur] != '\0') {
        error_print_custom("too many tokens in one command");
        return -1;
    }

    return num_tokens;
}

void trim (char *str)
{
    int i;
    int len = strlen(str);

    if (str == NULL)
        return;

    /* remove trailing spaces */
    for (i = len - 1; i >= 0; i--)
        if (str[i] != ' ')
            break;

    str[i + 1] = '\0';
    return;
}

void cleanup_tokens (char **tokens, int num_tokens)
{
    int i;
    if (m_tokens_f)
    {
        for (i = 0; i < num_tokens; i++)
            free(tokens[i]);
        free(tokens);
        m_tokens_f = 0;
    }
}

void cleanup_pipes (int **pipes, int num_pipes)
{
    int i;
    if (m_pipe_fds_f)
    {
        for (i = 0; i < num_pipes; i++)
            free(pipes[i]);
        free(pipes);
        m_pipe_fds_f = 0;
    }
}

void cleanup_pids (pid_t *pids)
{
    if (m_pids_f)
    {
        free(pids);
        m_pids_f = 0;
    }
}

/* input does not contain pipes */
/* it only consists of redirections or no redirections */
void handle_redirect(char *command)
{
    size_t i, j;
    char buffer[MAX_TOKEN_LEN];
    char file_in[MAX_TOKEN_LEN];
    char file_out[MAX_TOKEN_LEN];
    size_t len;
    int infile, outfile;
    int fd_in, fd_out;

    memset(buffer, 0, sizeof(buffer));
    memset(file_in, 0, sizeof(file_in));
    memset(file_out, 0, sizeof(file_out));

    len = strlen(command);
    infile = 0;
    outfile = 0;

    /* look for < */
    for (i = 0; i < len; ++i)
    {
        if (command[i] == '>')
        {
            /* copy the file name */
            strncpy(file_out, command + i + 1, len - i - 1);
            file_out[len - i - 1] = '\0';

            /* remove if any operator left */
            for (j = 0; j < strlen(file_out); ++j)
                if (file_out[j] == '<' || file_out[j] == '>')
                    file_out[j] = '\0';

            /* remove trailing spaces */
            trim(file_out);

            /* remove leading spaces */
            j = 0;
            while (file_out[j] == ' ')
                j++;
            
            if (j != 0)
                memmove(file_out, file_out + j, strlen(file_out) - j + 1);
            file_out[len - i - j] = '\0';

            /* set flag */
            outfile = 1;
        }
    }

    /* look for > */
    for (i = len - 1; i > 0; --i)
    {
        if (command[i] == '<')
        {
            /* copy the file name */
            strncpy(file_in, command + i + 1, len - i - 1);
            file_in[len - i - 1] = '\0';

            /* remove if any operator left */
            for (j = 0; j < len; ++j)
                if (file_in[j] == '<' || file_in[j] == '>')
                    file_in[j] = '\0';

            /* remove trailing spaces */
            trim(file_in);

            /* remove leading spaces */
            j = 0;
            while (file_in[j] == ' ')
                j++;
            
            if (j != 0)
                memmove(file_in, file_in + j, strlen(file_in) - j + 1);
            file_in[len - i - j] = '\0';

            /* set flag */
            infile = 1;
        }
    }

    if (!infile && !outfile)
        strcpy(buffer, command);
    else
    {
        /* get command */
        for (i = 0; i < len; ++i)
        {
            if (command[i] == '\0' || command[i] == '<' || command[i] == '>')
            {
                strncpy(buffer, command, i);
                buffer[i] = '\0';
                break;
            }
        }
    }

    if (infile)
    {
        fd_in = open(file_in, O_RDONLY);
        if (fd_in == -1)
            error_exit("open");
        if (dup2(fd_in, STDIN_FILENO) == -1)
            error_exit("dup2");
        if (close(fd_in) == -1)
            error_exit("close");
    }

    if (outfile)
    {
        fd_out = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out == -1)
            error_exit("open");
        if (dup2(fd_out, STDOUT_FILENO) == -1)
            error_exit("dup2");
        if (close(fd_out) == -1)
            error_exit("close");
    }

    parse_and_execute(buffer);
}

void parse_and_execute(char *command)
{
    char *args[MAX_TOKENS];
    char buffer[MAX_TOKEN_LEN];
    int num_tokens = 0;
    size_t start, end;
    size_t len = strlen(command);

    /* parse command according to spaces */
    for (start = 0; start < len; ++start)
    {
        if (command[start] == ' ')
            continue;

        for (end = start + 1; end < len; ++end)
        {
            if (command[end] == ' ')
                break;
        }

        /* copy to buffer */
        strncpy(buffer, command + start, end - start);
        buffer[end - start] = '\0';

        args[num_tokens] = malloc(strlen(buffer) + 1);
        if (args[num_tokens] == NULL)
            error_exit("malloc6");

        strncpy(args[num_tokens], buffer, strlen(buffer) + 1);
        num_tokens++;

        start = end;
    }

    args[num_tokens] = NULL;

    log_and_execute(args);
}

void log_and_execute(char **args) {
    char buffer[50];
    char filename[50];
    char errmsg[50];
    int fd, i;
    pid_t pid;
    struct flock lock;
    char pid_str[10];
    struct stat st = {0};

    memset(buffer, 0, sizeof(buffer));
    memset(filename, 0, sizeof(filename));
    memset(errmsg, 0, sizeof(errmsg));

    get_timestamp(buffer);
    
    /* create log directory if it does not exist */
    if (stat("log", &st) == -1) {
        mkdir("log", 0700);
    }

    /* add .txt extension */
    strcat(buffer, ".txt");

    strcpy(filename, "log/");
    strcat(filename, buffer);

    /* open file continuously if there is EINTR */
    while (1) {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else
                error_exit("open");
        }
        break;
    }

    pid = getpid();
    

    /* set lock using fcntl */
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = pid;

    if (fcntl(fd, F_SETLKW, &lock) == -1)
        error_exit("fcntl");

    /* write pid */
    if (write(fd, "\npid: ", 5) == -1)
        error_exit("write");
    
    sprintf(pid_str, "%d\n", pid);

    if (write(fd, pid_str, strlen(pid_str)) == -1)
        error_exit("write");

    for (i = 0; args[i] != NULL; ++i) {
        if (write(fd, args[i], strlen(args[i])) == -1)
            error_exit("write");
        if (write(fd, " ", 1) == -1)
            error_exit("write");
    }

    execvp(args[0], args);

    fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

    /* set lock using fcntl */
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = pid;

    if (fcntl(fd, F_SETLKW, &lock) == -1)
        error_exit("fcntl");

    /* write error message */
    if (write(fd, "\nerror: ", 8) == -1)
        error_exit("write");
    
    strcpy(errmsg, strerror(errno));
    
    if(strlen(errmsg) > 49)
        errmsg[49] = '\0';
    else {
        /* add newline */
        errmsg[strlen(errmsg)] = '\n';
        errmsg[strlen(errmsg) + 1] = '\0';
    }

    if (write(fd, errmsg, strlen(errmsg)) == -1)
        error_exit("write");

    close(fd);

    /* free memory */
    for (i = 0; args[i] != NULL; ++i)
        free(args[i]);

    error_exit("exec");
}


int check_term_signal (int status, pid_t pid)
{
    if (WIFSIGNALED(status))
    {
        /* if parent get the signal */
        if (parent_get_sig)
        {
            if (WTERMSIG(status) == SIGINT) {
                printf("\nParent process get SIGINT, all childs are terminated\n");
                sigint_count = 0;
            }
                
            else if (WTERMSIG(status) == SIGQUIT) {
                printf("\nParent process get SIGQUIT, all childs are terminated\n");
                sigquit_count = 0;
            }
            else if (WTERMSIG(status) == SIGTERM) {
                printf("\nParent process get SIGTERM, all childs are terminated\n");
                sigterm_count = 0;
            }
            
            sig_raised = 0;
            parent_get_sig = 0;
            return 0;
        }

        /* if child was killed by SIGKILL */
        else if (WTERMSIG(status) == SIGKILL)
        {
            printf("\nChild process %d was killed by SIGKILL\n", pid);
            return 1;
        }
        else if (WTERMSIG(status) == SIGINT)
            printf("\nChild process %d was killed by SIGINT\n", pid);
        else if (WTERMSIG(status) == SIGQUIT)
            printf("\nChild process %d was killed by SIGQUIT\n", pid);
        else if (WTERMSIG(status) == SIGTERM)
            printf("\nChild process %d was killed by SIGTERM\n", pid);
        
        return 0;
    }

    return -1;
}