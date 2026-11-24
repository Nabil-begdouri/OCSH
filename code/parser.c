#include <ctype.h>
#include <string.h>

#include "parser.h"

int split_pipeline(char *input, char *commands[], int max_commands)
{
    if (input == NULL || commands == NULL || max_commands <= 0)
        return -1;

    int count = 0;
    char *start = input;
    char quote = '\0';

    for (char *p = input; ; p++)
    {
        if (*p == '\0')
        {
            if (quote != '\0')
                return -1;

            char *end = p;

            while (isspace((unsigned char)*start))
                start++;

            while (end > start && isspace((unsigned char)*(end - 1)))
                end--;

            *end = '\0';

            if (*start == '\0' || count >= max_commands)
                return -1;

            commands[count++] = start;
            break;
        }

        if ((*p == '"' || *p == '\'') &&
            (quote == '\0' || quote == *p))
        {
            if (quote == '\0')
                quote = *p;
            else
                quote = '\0';

            continue;
        }

        if (quote == '\0' && p[0] == '-' && p[1] == '>')
        {
            *p = '\0';

            char *end = p;

            while (isspace((unsigned char)*start))
                start++;

            while (end > start && isspace((unsigned char)*(end - 1)))
                end--;

            *end = '\0';

            if (*start == '\0' || count >= max_commands)
                return -1;

            commands[count++] = start;

            p++;
            start = p + 1;
        }
    }

    return count;
}
