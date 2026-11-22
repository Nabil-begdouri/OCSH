#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "execution.h"

int main()
{
    char command[100];
    char *args[20];

    while (1)
    {
        printf("ocsh> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
            break;

        command[strcspn(command, "\n")] = '\0';

        if (strlen(command) == 0)
            continue;

        if (strcmp(command, "exit") == 0)
        {
            printf("Goodbye!\n");
            break;
        }

        int i = 0;
        char *token = strtok(command, " ");

        while (token != NULL && i < 19)
        {
            args[i] = token;
            i++;
            token = strtok(NULL, " ");
        }

        args[i] = NULL;

        execute_command(args);
    }

    return 0;
}