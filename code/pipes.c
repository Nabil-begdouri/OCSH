#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pipes.h"
#include "parser.h"
#include "shell.h"
#include "jobs.h"
#include "redirections.h"
#include "builtins.h"

int execute_pipeline(char *commands[], int command_count,
                     int background, const char *label, pid_t *last_pid)
{
    if (commands == NULL || command_count <= 0)
        return 1;

    if (command_count > MAX_COMMANDS)
    {
        fprintf(stderr, "ocsh: too many commands in pipeline\n");
        return 1;
    }

    if (background && !jobs_has_free_slot())
    {
        fprintf(stderr, "ocsh: too many background jobs\n");
        return 1;
    }

    sigset_t block_mask;
    sigset_t old_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1)
    {
        perror("ocsh: sigprocmask");
        return 1;
    }

    int previous_fd = -1;
    int pipe_fd[2];
    pid_t pids[MAX_COMMANDS];
    int started = 0;
    pid_t pgid = 0;

    for (int i = 0; i < command_count; i++)
    {
        char *args[MAX_ARGS];

        tokenize(commands[i], args, MAX_ARGS);

        if (args[0] == NULL)
        {
            fprintf(stderr, "ocsh: empty command in pipeline\n");
            goto error;
        }

        if (i < command_count - 1)
        {
            if (pipe(pipe_fd) == -1)
            {
                perror("ocsh: pipe");
                goto error;
            }
        }

        pid_t pid = fork();

        if (pid == -1)
        {
            perror("ocsh: fork");

            if (i < command_count - 1)
            {
                close(pipe_fd[0]);
                close(pipe_fd[1]);
            }

            goto error;
        }

        if (pid == 0)
        {
            sigprocmask(SIG_SETMASK, &old_mask, NULL);

            setpgid(0, pgid);

            if (!background && jobs_interactive())
                tcsetpgrp(STDIN_FILENO, getpgrp());

            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            if (background && previous_fd == -1)
            {
                int null_fd = open("/dev/null", O_RDONLY);

                if (null_fd != -1)
                {
                    dup2(null_fd, STDIN_FILENO);
                    close(null_fd);
                }
            }

            if (previous_fd != -1)
            {
                if (dup2(previous_fd, STDIN_FILENO) == -1)
                {
                    perror("ocsh: dup2");
                    _exit(126);
                }

                close(previous_fd);
            }

            if (i < command_count - 1)
            {
                close(pipe_fd[0]);

                if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
                {
                    perror("ocsh: dup2");
                    _exit(126);
                }

                close(pipe_fd[1]);
            }

            /* Redirections */
            if (handle_redirection(args) == -1)
                _exit(1);

            /* Builtin dans un pipeline ou en arriere-plan (say hi -> wc -c,
               osinfo &) : il s'execute dans ce fils, puis le fils se termine. */
            int builtin_status = execute_builtin_in_child(args);

            if (builtin_status != -1)
            {
                fflush(stdout);
                _exit(builtin_status);
            }

            execvp(args[0], args);

            fprintf(stderr, "ocsh: %s: %s\n",
                    args[0], strerror(errno));
            _exit(127);
        }

        if (pgid == 0)
            pgid = pid;

        setpgid(pid, pgid);

        pids[started++] = pid;

        if (previous_fd != -1)
            close(previous_fd);

        if (i < command_count - 1)
        {
            close(pipe_fd[1]);
            previous_fd = pipe_fd[0];
        }
    }

    if (background)
    {
        int job_id = jobs_add(pids, started, label, pgid, 0);

        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        if (last_pid != NULL)
            *last_pid = pids[started - 1];

        printf("[%d] %d\n", job_id, (int)pids[started - 1]);
        return 0;
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return jobs_wait_foreground(pgid, pids, started, label);

error:

    if (previous_fd != -1)
        close(previous_fd);

    for (int i = 0; i < started; i++)
    {
        kill(pids[i], SIGTERM);

        while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR)
        {
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return 1;
}