#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
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
        /* Le shell ignore SIGINT ; "ignore" survit a exec, donc le fils
           doit remettre le comportement par defaut pour que Ctrl+C
           puisse le tuer. */
        signal(SIGINT, SIG_DFL);

        execvp(args[0], args);
        fprintf(stderr, "ocsh: %s: %s\n", args[0], strerror(errno));
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
    {
        /* Ctrl+C : le fils est mort, le shell reste en vie. */
        if (WTERMSIG(status) == SIGINT)
            printf("\n");

        return 128 + WTERMSIG(status);
    }

    return 1;
}
