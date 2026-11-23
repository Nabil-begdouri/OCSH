#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "jobs.h"

int execute_background(char *args[])
{
    if (args == NULL || args[0] == NULL)
        return 1;

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

    printf("[%d] %d\n", 1, pid);

    return 0;
}