#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "jobs.h"

#define MAX_BACKGROUND_JOBS 128

static pid_t background_pids[MAX_BACKGROUND_JOBS];

static void handle_sigchld(int signal_number)
{
    (void)signal_number;

    int saved_errno = errno;
    int status;

    for (int i = 0; i < MAX_BACKGROUND_JOBS; i++)
    {
        if (background_pids[i] > 0)
        {
            pid_t result;

            do
            {
                result = waitpid(background_pids[i], &status, WNOHANG);
            }
            while (result == -1 && errno == EINTR);

            if (result == background_pids[i] ||
                (result == -1 && errno == ECHILD))
            {
                background_pids[i] = 0;
            }
        }
    }

    errno = saved_errno;
}

void setup_jobs(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigchld;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &action, NULL) == -1)
        perror("ocsh: sigaction");
}

int execute_background(char *args[], pid_t *last_background_pid)
{
    if (args == NULL || args[0] == NULL)
        return 1;

    sigset_t block_mask;
    sigset_t old_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1)
    {
        perror("ocsh: sigprocmask");
        return 1;
    }

    int slot = -1;

    for (int i = 0; i < MAX_BACKGROUND_JOBS; i++)
    {
        if (background_pids[i] == 0)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        fprintf(stderr, "ocsh: too many background jobs\n");
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("ocsh: fork");
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return 1;
    }

    if (pid == 0)
    {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        signal(SIGCHLD, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        execvp(args[0], args);
        perror("ocsh");
        _exit(127);
    }

    background_pids[slot] = pid;

    if (last_background_pid != NULL)
        *last_background_pid = pid;

    printf("[%d] %d\n", slot + 1, pid);

    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1)
    {
        perror("ocsh: sigprocmask");
        return 1;
    }

    return 0;
}