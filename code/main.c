#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "execution.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

int main()
{
    char command[MAX_COMMAND_LENGTH];
    char *args[MAX_ARGS];
    int last_status = 0;

    while (1)
    {
        printf("ocsh> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        command[strcspn(command, "\n")] = '\0';
        if (strcmp(command, "echo $?") == 0)
        {
            printf("%d\n", last_status);
            continue;
        }

        int i = 0;
        char *token = strtok(command, " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i] = token;
            i++;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        if (i == 0)
            continue;

        if (strcmp(args[0], "exit") == 0)
        {
            printf("Goodbye!\n");
            break;
        }

        last_status = execute_command(args);
    }

    return 0;
}