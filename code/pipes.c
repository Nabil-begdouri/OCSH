#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "pipes.h"
#include "parser.h"

#define MAX_ARGS 64

int execute_pipeline(char *commands[], int command_count)
{
    if (commands == NULL || command_count <= 0)
        return 1;

    if (command_count > MAX_COMMANDS)
    {
        fprintf(stderr, "ocsh: too many commands in pipeline\n");
        return 1;
    }

    int previous_fd = -1;
    int pipe_fd[2];
    int status;
    pid_t pids[MAX_COMMANDS];
    int started = 0;

    for (int i = 0; i < command_count; i++)
    {
        char *args[MAX_ARGS];

        /* Decoupage en mots en respectant les guillemets */
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
            signal(SIGINT, SIG_DFL);

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

            execvp(args[0], args);

            perror("ocsh");
            _exit(127);
        }

        pids[started++] = pid;

        if (previous_fd != -1)
            close(previous_fd);

        if (i < command_count - 1)
        {
            close(pipe_fd[1]);
            previous_fd = pipe_fd[0];
        }
    }

    int last_status = 0;

    for (int i = 0; i < started; i++)
    {
        pid_t result;

        do
        {
            result = waitpid(pids[i], &status, 0);
        }
        while (result == -1 && errno == EINTR);

        if (result == -1)
        {
            perror("ocsh: waitpid");
            last_status = 1;
        }
        else if (i == started - 1)
        {
            if (WIFEXITED(status))
                last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                last_status = 128 + WTERMSIG(status);
        }
    }

    return last_status;

error:

    if (previous_fd != -1)
        close(previous_fd);

    for (int i = 0; i < started; i++)
    {
        pid_t result;

        do
        {
            result = waitpid(pids[i], NULL, 0);
        }
        while (result == -1 && errno == EINTR);
    }

    return 1;
}