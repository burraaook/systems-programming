#include "mycommon.h"
#include <sys/wait.h>

#ifndef _SHELL_IMPL_H_
#define _SHELL_IMPL_H_

#define MAX_TOKEN_LEN 250
#define MAX_TOKENS 350

/* signal flags */
extern sig_atomic_t sig_raised;
extern sig_atomic_t sigint_count;
extern sig_atomic_t sigterm_count;
extern sig_atomic_t sigquit_count;
extern sig_atomic_t sigusr1_count;
extern sig_atomic_t sigusr2_count;
extern sig_atomic_t sigtstp_count;
extern sig_atomic_t child_f;
extern sig_atomic_t cur_child;
extern sig_atomic_t parent_get_sig;

/*
* it is a shell command executer implementation which is implemented using fork-exec
* returns 1 if child process gets sigkill
*/
int shell_impl (const char *command);

/*
* tokenizes the command string into tokens
*/
int command_tokenizer (const char *command, char **tokens, char delim);

/*
* Trims trailing spaces
*/
void trim (char *str);

/*
* memory cleanup functions
*/
void cleanup_tokens (char **tokens, int num_tokens);
void cleanup_pipes (int **pipes, int num_pipes);
void cleanup_pids (pid_t *pids);

/*
* handles redirect operator using dup2
*/
void handle_redirect (char *command);

/*
* parses and executes the command, makes ready for exec parameters
*/
void parse_and_execute (char *command);

/*
* logs the command and executes it
*/
void log_and_execute (char **args);

/*
* checks child termination status
*/
int check_term_signal (int status, pid_t pid);


#endif 
