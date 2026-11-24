#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "execution.h"
#include "history.h"
#include "completion.h"
#include "variables.h"
#include "command_substitution.h"
#include "oschrc.h"

#define MAX_ARGS 64

int main(void)
{
    load_oschrc();

    char command[MAX_COMMAND_LENGTH];
    char expanded_command[MAX_COMMAND_LENGTH];
    int last_status = 0;

    while (1)
    {
        rl_attempted_completion_function = command_completion;

        char *input_line = readline("ocsh> ");

        if (input_line == NULL)
        {
            printf("\n");
            break;
        }

        add_history(input_line);

        strncpy(command, input_line, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';

        free(input_line);

        add_custom_history(command);

        char command_substituted[MAX_COMMAND_LENGTH];

        expand_command_substitution(
            command,
            command_substituted,
            sizeof(command_substituted)
        );

        expand_variables(
            command_substituted,
            expanded_command,
            sizeof(expanded_command),
            last_status
        );

        strcpy(command, expanded_command);

        char *argv[MAX_ARGS];
        int i = 0;

        char *token = strtok(command, " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            argv[i] = token;
            i++;
            token = strtok(NULL, " \t");
        }

        argv[i] = NULL;

        if (i == 0)
            continue;

        if (strcmp(argv[0], "past") == 0)
        {
            display_custom_history();
            continue;
        }

        if (strcmp(argv[0], "quit") == 0)
        {
            printf("Goodbye!\n");
            break;
        }

        if (strcmp(argv[0], "export") == 0)
        {
            last_status = handle_export(argv[1]);
            continue;
        }

        last_status = execute_command(argv);
    }

    return 0;
}