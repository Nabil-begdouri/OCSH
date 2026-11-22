#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "execution.h"

void execute_command(char **args)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("ocsh: fork");
        return;
    }

    if (pid == 0)
    {
        execvp(args[0], args);

        perror("ocsh");
        exit(EXIT_FAILURE);
    }
    else
    {
        waitpid(pid, NULL, 0);
    }
}
