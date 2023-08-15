#include "include/mycommon.h"
#include "include/shell_impl.h"
#include "signal.h"

#define MAX_COMMAND_LEN 4096

sig_atomic_t sig_raised = 0;
sig_atomic_t sigint_count = 0;
sig_atomic_t sigterm_count = 0;
sig_atomic_t sigquit_count = 0;
sig_atomic_t sigusr1_count = 0;
sig_atomic_t sigusr2_count = 0;
sig_atomic_t sigtstp_count = 0;
sig_atomic_t child_f = 0;
sig_atomic_t cur_child = 0;
sig_atomic_t parent_get_sig = 0;

void signal_print();
void get_command (char *command);
void set_signal_handler();

/* handler for signals */
void handler(int signal_number)
{
    switch (signal_number)
    {
        case SIGINT:
            ++sigint_count;
            if (child_f == 1)
                kill(cur_child, SIGINT);

            /* set flags */
            child_f = 0;
            cur_child = 0;
            parent_get_sig = 1;
            break;
        case SIGTERM:
            ++sigterm_count;
            if (child_f == 1)
                kill(cur_child, SIGTERM);

            /* set flags */
            child_f = 0;
            cur_child = 0;
            parent_get_sig = 1;
            break;
        case SIGQUIT:
            ++sigquit_count;
            if (child_f == 1)
                kill(cur_child, SIGQUIT);

            /* set flags */
            child_f = 0;
            cur_child = 0;
            parent_get_sig = 1;
            break;
        case SIGUSR1:
            ++sigusr1_count;
            break;
        case SIGUSR2:
            ++sigusr2_count;
            break;
        case SIGTSTP:
            ++sigtstp_count;
            break;
    }
    sig_raised++;
}

int main()
{
    char *command = NULL;
    int status;

    /* set the signal handler */
    set_signal_handler();

    command = (char *)malloc((MAX_COMMAND_LEN + 1) * sizeof(char));

    /* initialize command */
    memset(command, 0, (MAX_COMMAND_LEN + 1) * sizeof(char));

    /* set stdout to unbuffered */
    setbuf(stdout, NULL);
 
    while (1)
    {   
        /* print signal if raised */
        if (sig_raised > 0)
        {
            signal_print();
            sig_raised = 0;
        }

        fprintf(stdout, "$ ");

        /* get command */
        get_command(command);
        
        if (strcmp(command, "\n") == 0)
            continue;   

        if (command[strlen(command) - 1] == '\n')
            command[strlen(command) - 1] = '\0';
    
        if (strcmp(command, ":q") == 0)
            break;

        /* print signal if raised, and don't execute command */
        if (sig_raised > 0)
        {
            signal_print();
            sig_raised = 0;
            continue;
        }

        status = shell_impl(command);

        /* means sigkill happened for child */
        if (status == 1)
            break;
        
    }

    free(command);

    return 0;
}

/* print signal type and reset the counter */
void signal_print()
{
    int flag = 1;
    if (sigint_count > 0) {
        fprintf(stdout, "\nSIGINT was raised\n");
        sigint_count = 0;
        flag = 0;
    }
    if (sigterm_count > 0) {
        fprintf(stdout, "\nSIGTERM was raised\n");
        sigterm_count = 0;
        flag = 0;
    }
    if (sigquit_count > 0) {
        fprintf(stdout, "\nSIGQUIT was raised\n");
        sigquit_count = 0;
        flag = 0;
    }
    if (sigusr1_count > 0) {
        fprintf(stdout, "\nSIGUSR1 was raised\n");
        sigusr1_count = 0;
        flag = 0;
    }
    if (sigusr2_count > 0) {
        fprintf(stdout, "\nSIGUSR2 was raised\n");
        sigusr2_count = 0;
        flag = 0;
    }
    if (sigtstp_count > 0) {
        fprintf(stdout, "\nSIGTSTP was raised\n");
        sigtstp_count = 0;
        flag = 0;
    }

    if (flag)
        fprintf(stdout, "\nsignal was raised\n");
}

void get_command (char *command)
{
    struct sigaction sa, old_sa;

    /* set up the signal handler with SA_RESTART flag turned off */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &old_sa);
    sigaction(SIGTERM, &sa, &old_sa);
    sigaction(SIGQUIT, &sa, &old_sa);
    sigaction(SIGABRT, &sa, &old_sa);
    sigaction(SIGALRM, &sa, &old_sa);
    sigaction(SIGBUS, &sa, &old_sa);
    sigaction(SIGPIPE, &sa, &old_sa);
    sigaction(SIGUSR1, &sa, &old_sa);
    sigaction(SIGUSR2, &sa, &old_sa);
    sigaction(SIGTSTP, &sa, &old_sa);

    strcpy(command, "------");
    fgets(command, MAX_COMMAND_LEN, stdin);

    /* restore the old signal handler */
    sigaction(SIGINT, &old_sa, NULL);
    sigaction(SIGTERM, &old_sa, NULL);
    sigaction(SIGQUIT, &old_sa, NULL);
    sigaction(SIGABRT, &old_sa, NULL);
    sigaction(SIGALRM, &old_sa, NULL);
    sigaction(SIGBUS, &old_sa, NULL);
    sigaction(SIGPIPE, &old_sa, NULL);
    sigaction(SIGUSR1, &old_sa, NULL);
    sigaction(SIGUSR2, &old_sa, NULL);
    sigaction(SIGTSTP, &old_sa, NULL);
}


void set_signal_handler()
{
    struct sigaction sa;

    /* set up signal handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler;
    sa.sa_flags = SA_RESTART;

    /* handler for all signals */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

}