#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "execution.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define HISTORY_SIZE 100

char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
int history_count = 0;

void expand_variables(char *input, char *output, size_t size, int last_status)
{
    size_t i = 0;
    size_t j = 0;

    if (size == 0)
        return;

    while (input[i] != '\0' && j < size - 1)
    {
        if (input[i] == '$')
        {
            i++;

            if (input[i] == '?')
            {
                char status[16];
                snprintf(status, sizeof(status), "%d", last_status);

                size_t length = strlen(status);

                if (length > size - 1 - j)
                    length = size - 1 - j;

                memcpy(output + j, status, length);
                j += length;
                i++;
                continue;
            }

            char variable[128];
            size_t k = 0;

            if (input[i] == '{')
            {
                i++;

                while (input[i] != '\0' && input[i] != '}' && k < sizeof(variable) - 1)
                    variable[k++] = input[i++];

                if (input[i] == '}')
                    i++;
            }
            else
            {
                while ((isalnum((unsigned char)input[i]) || input[i] == '_') && k < sizeof(variable) - 1)
                    variable[k++] = input[i++];
            }

            variable[k] = '\0';

            if (k > 0)
            {
                char *value = getenv(variable);

                if (value != NULL)
                {
                    size_t length = strlen(value);

                    if (length > size - 1 - j)
                        length = size - 1 - j;

                    memcpy(output + j, value, length);
                    j += length;
                }
            }
        }
        else
        {
            output[j++] = input[i++];
        }
    }

    output[j] = '\0';
}

int main()
{
    char command[MAX_COMMAND_LENGTH];
    char expanded_command[MAX_COMMAND_LENGTH];
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
        if (history_count < HISTORY_SIZE)
{
    strcpy(history[history_count], command);
    history_count++;
}

        expand_variables(command, expanded_command, sizeof(expanded_command), last_status);
        strcpy(command, expanded_command);

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
        if (strcmp(args[0], "past") == 0)
{
    for (int j = 0; j < history_count; j++)
    {
        printf("%d  %s\n", j + 1, history[j]);
    }
    continue;
}

        if (strcmp(args[0], "quit") == 0)
        {
            printf("Goodbye!\n");
            break;
        }

        if (strcmp(args[0], "export") == 0)
        {
            if (args[1] == NULL || strchr(args[1], '=') == NULL)
            {
                printf("Usage: export NOM=valeur\n");
                last_status = 1;
            }
            else
            {
                char *equal = strchr(args[1], '=');
                *equal = '\0';

                if (setenv(args[1], equal + 1, 1) != 0)
                {
                    perror("export");
                    last_status = 1;
                }
                else
                {
                    last_status = 0;
                }
            }

            continue;
        }

        last_status = execute_command(args);
    }

    return 0;
}