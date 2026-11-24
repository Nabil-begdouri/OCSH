#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"

int execute_builtin(char **args)
{
    if (args == NULL || args[0] == NULL)
        return 0;

    if (strcmp(args[0], "list") == 0)
    {
        args[0] = "ls";
        return -1;
    }

    if (strcmp(args[0], "show") == 0)
    {
        args[0] = "cat";
        return -1;
    }

    if (strcmp(args[0], "copy") == 0)
    {
        args[0] = "cp";
        return -1;
    }

    if (strcmp(args[0], "move") == 0)
    {
        args[0] = "mv";
        return -1;
    }

    if (strcmp(args[0], "del") == 0)
    {
        args[0] = "rm";
        return -1;
    }

    if (strcmp(args[0], "newdir") == 0)
    {
        args[0] = "mkdir";
        return -1;
    }

    if (strcmp(args[0], "newfile") == 0)
    {
        args[0] = "touch";
        return -1;
    }

    if (strcmp(args[0], "go") == 0)
{
    char *home = getenv("HOME");

    if (args[1] == NULL || strcmp(args[1], "~") == 0)
    {
        if (home == NULL || chdir(home) != 0)
        {
            perror("cd");
            return 1;
        }
    }
    else
    {
        
        if (chdir(args[1]) != 0)
        {
            perror("cd");
            return 1;
        }
        
    }

    return 0;
}

    if (strcmp(args[0], "path") == 0)
    {
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("pwd");
            return 1;
        }

        printf("%s\n", cwd);
        return 0;
    }

    if (strcmp(args[0], "info") == 0)
{
    printf("OCSH - Built-in commands\n");
    printf("  go    : change directory\n");
    printf("  quit  : exit shell\n");
    printf("  path  : display current directory\n");
    printf("  say   : display text\n");
    printf("  info  : display help\n");
    printf("  past  : display command history\n");
    printf("  pipe  : connect commands\n");
    printf("  osinfo: display OS information\n");
    return 0;
}
    if (strcmp(args[0], "say") == 0)
    {
        for (int i = 1; args[i] != NULL; i++)
        {
            printf("%s", args[i]);

            if (args[i + 1] != NULL)
                printf(" ");
        }

        printf("\n");
        return 0;
    }
    if (strcmp(args[0], "osinfo") == 0)
    {
        printf("Systèmes d’Exploitation -- UIR/ESIN\n");
        return 0;
    }



    return -1;
}