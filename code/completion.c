#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include "completion.h"

char *command_generator(const char *text, int state)
{
    static int index;

    static char *commands[] = {
        "go", "path", "info", "say", "osinfo",
        "quit", "past", "export", "list", "show",
        "copy", "move", "del", "newdir", "newfile",
        NULL
    };

    if (!state)
        index = 0;

    while (commands[index])
    {
        char *command = commands[index++];

        if (strncmp(command, text, strlen(text)) == 0)
            return strdup(command);
    }

    return NULL;
}

char **command_completion(const char *text, int start, int end)
{
    (void)start;
    (void)end;

    return rl_completion_matches(text, command_generator);
}