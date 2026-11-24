#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "variables.h"

void expand_variables(char *input, char *output, size_t size,
                      int last_status, pid_t last_background_pid)
{
    if (input == NULL || output == NULL || size == 0)
        return;

    size_t i = 0;
    size_t j = 0;

    while (input[i] != '\0' && j < size - 1)
    {
        if (input[i] != '$')
        {
            output[j++] = input[i++];
            continue;
        }

        i++;

        if (input[i] == '?')
        {
            char value[16];
            snprintf(value, sizeof(value), "%d", last_status);

            size_t length = strlen(value);
            if (length > size - 1 - j)
                length = size - 1 - j;

            memcpy(output + j, value, length);
            j += length;
            i++;
            continue;
        }

        if (input[i] == '!')
        {
            char value[32];
            snprintf(value, sizeof(value), "%ld",
                     (long)last_background_pid);

            size_t length = strlen(value);
            if (length > size - 1 - j)
                length = size - 1 - j;

            memcpy(output + j, value, length);
            j += length;
            i++;
            continue;
        }

        char variable[128];
        size_t k = 0;

        if (input[i] == '{')
        {
            i++;

            while (input[i] != '\0' && input[i] != '}')
            {
                if (k >= sizeof(variable) - 1)
                    break;

                variable[k++] = input[i++];
            }

            if (input[i] == '}')
                i++;
        }
        else
        {
            while (isalnum((unsigned char)input[i])
                   || input[i] == '_')
            {
                if (k >= sizeof(variable) - 1)
                    break;

                variable[k++] = input[i++];
            }
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

    output[j] = '\0';
}

int execute_export(char *args[])
{
    if (args == NULL || args[1] == NULL ||
        args[2] != NULL || strchr(args[1], '=') == NULL)
    {
        printf("Usage: export NOM=valeur\n");
        return 1;
    }

    char *equal = strchr(args[1], '=');

    if (equal == args[1])
    {
        printf("export: invalid variable name\n");
        return 1;
    }

    for (char *p = args[1]; p < equal; p++)
    {
        if ((!isalnum((unsigned char)*p) && *p != '_') ||
            (p == args[1] && isdigit((unsigned char)*p)))
        {
            printf("export: invalid variable name\n");
            return 1;
        }
    }

    *equal = '\0';

    if (setenv(args[1], equal + 1, 1) != 0)
    {
        perror("export");
        return 1;
    }

    return 0;
}
