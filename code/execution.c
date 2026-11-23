#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "execution.h"

int execute_command(char **args)
{
    if (args == NULL || args[0] == NULL)
        return 0;

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("ocsh: fork");
        return 1;
    }

    if (pid == 0)
    {
        execvp(args[0], args);
        perror("ocsh");
        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) == -1)
    {
        perror("ocsh: waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}