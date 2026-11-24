#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oschrc.h"
#include "execution.h"
#include "history.h"

#define MAX_ARGS 64

void load_oschrc(void)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/.oschrc", getenv("HOME"));

    FILE *file = fopen(path, "r");

    if (file == NULL)
        return;

    char line[MAX_COMMAND_LENGTH];

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#')
            continue;

        char *args[MAX_ARGS];
        int i = 0;

        char *token = strtok(line, " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i++] = token;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        if (i > 0)
            execute_command(args);
    }

    fclose(file);
}