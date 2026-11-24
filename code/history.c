#include <stdio.h>
#include <string.h>
#include "history.h"

static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;

void add_custom_history(const char *command)
{
    if (history_count == HISTORY_SIZE)
    {
        memmove(history[0], history[1],
                (HISTORY_SIZE - 1) * MAX_COMMAND_LENGTH);
        history_count--;
    }

    {
        strncpy(history[history_count], command, MAX_COMMAND_LENGTH - 1);
        history[history_count][MAX_COMMAND_LENGTH - 1] = '\0';
        history_count++;
    }
}

void display_custom_history(void)
{
    for (int i = 0; i < history_count; i++)
    {
        printf("%d  %s\n", i + 1, history[i]);
    }
}