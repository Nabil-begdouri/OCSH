#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variables.h"

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

                while (input[i] != '\0' &&
                       input[i] != '}' &&
                       k < sizeof(variable) - 1)
                {
                    variable[k++] = input[i++];
                }

                if (input[i] == '}')
                    i++;
            }
            else
            {
                while ((isalnum((unsigned char)input[i]) ||
                        input[i] == '_') &&
                       k < sizeof(variable) - 1)
                {
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
        else
        {
            output[j++] = input[i++];
        }
    }

    output[j] = '\0';
}

int handle_export(char *argument)
{
    if (argument == NULL || strchr(argument, '=') == NULL)
    {
        printf("Usage: export NOM=valeur\n");
        return 1;
    }

    char *equal = strchr(argument, '=');
    *equal = '\0';

    if (setenv(argument, equal + 1, 1) != 0)
    {
        perror("export");
        return 1;
    }

    return 0;
}