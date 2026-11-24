#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "execution.h"
#include "parser.h"
#include "pipes.h"
#include "jobs.h"
#include "variables.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

int main(void)
{
    /* Le shell ignore Ctrl+C pour ne pas se fermer. */
    signal(SIGINT, SIG_IGN);

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
            last_status = execute_export(args);
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
