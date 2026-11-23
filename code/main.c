#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "execution.h"
#include "parser.h"
#include "pipes.h"
#include "jobs.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

void expand_variables(char *input, char *output, size_t size,
                      int last_status, pid_t last_background_pid)
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

            if (input[i] == '!')
            {
                char pid[32];
                snprintf(pid, sizeof(pid), "%ld",
                         (long)last_background_pid);

                size_t length = strlen(pid);
                if (length > size - 1 - j)
                    length = size - 1 - j;

                memcpy(output + j, pid, length);
                j += length;
                i++;
                continue;
            }

            char variable[128];
            size_t k = 0;

            if (input[i] == '{')
            {
                i++;

                while (input[i] != '\0' && input[i] != '}'
                       && k < sizeof(variable) - 1)
                    variable[k++] = input[i++];

                if (input[i] == '}')
                    i++;
            }
            else
            {
                while ((isalnum((unsigned char)input[i])
                        || input[i] == '_')
                       && k < sizeof(variable) - 1)
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

int main(void)
{
    setup_jobs();

    char command[MAX_COMMAND_LENGTH];
    char expanded_command[MAX_COMMAND_LENGTH];
    char *commands[MAX_COMMANDS];
    char *args[MAX_ARGS];

    int last_status = 0;
    pid_t last_background_pid = 0;

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

        expand_variables(command, expanded_command,
                         sizeof(expanded_command),
                         last_status, last_background_pid);

        int command_count = split_pipeline(
            expanded_command, commands, MAX_COMMANDS);

        if (command_count < 0)
        {
            fprintf(stderr,
                    "ocsh: invalid command or empty pipeline\n");
            last_status = 2;
            continue;
        }

        if (command_count > 1)
        {
            last_status = execute_pipeline(commands, command_count);
            continue;
        }

        int i = 0;
        char *token = strtok(commands[0], " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i++] = token;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        if (i == 0)
            continue;

        int background = 0;

        if (strcmp(args[i - 1], "&") == 0)
        {
            background = 1;
            args[i - 1] = NULL;
            i--;
        }

        if (i == 0)
            continue;

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

        if (background)
        {
            last_status = execute_background(
                args, &last_background_pid);
        }
        else
        {
            last_status = execute_command(args);
        }
    }

    return 0;
}