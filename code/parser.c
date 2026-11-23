#include <ctype.h>
#include <string.h>
#include "parser.h"

int split_pipeline(char *input, char *commands[], int max_commands)
{
    if (input == NULL || commands == NULL || max_commands <= 0)
        return -1;

    int count = 0;
    char *start = input;

    while (1)
    {
        char *separator = strstr(start, "->");

        if (separator != NULL)
            *separator = '\0';

        while (isspace((unsigned char)*start))
            start++;

        char *end = start + strlen(start);

        while (end > start && isspace((unsigned char)*(end - 1)))
            end--;

        *end = '\0';

        if (*start == '\0' || count >= max_commands)
            return -1;

        commands[count++] = start;

        if (separator == NULL)
            break;

        start = separator + 2;
    }

    return count;
}