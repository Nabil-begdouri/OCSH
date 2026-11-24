#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "parser.h"
#include "pipes.h"
#include "jobs.h"
#include "script.h"
#include "shell.h"
#include "variables.h"
#include "builtins.h"
#include "history.h"
#include "completion.h"
#include "command_substitution.h"
#include "redirections.h"
#include "ocshrc.h"

int g_last_status = 0;
pid_t g_last_background_pid = 0;
int g_running = 1;

static int is_valid_name(const char *name, size_t length)
{
    if (length == 0)
        return 0;

    if (!(isalpha((unsigned char)name[0]) || name[0] == '_'))
        return 0;

    for (size_t i = 1; i < length; i++)
    {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
            return 0;
    }

    return 1;
}

static int run_builtin(char **args, int *status)
{
    if (strcmp(args[0], "quit") == 0)
    {
        printf("Goodbye!\n");
        g_running = 0;
        *status = g_last_status;
        return 1;
    }

    if (strcmp(args[0], "export") == 0)
    {
        *status = execute_export(args);
        return 1;
    }

    if (strcmp(args[0], "jobs") == 0)
    {
        jobs_print();
        *status = 0;
        return 1;
    }

    if (strcmp(args[0], "fg") == 0)
    {
        *status = jobs_fg(jobs_parse_id(args[1]));
        return 1;
    }

    if (strcmp(args[0], "bg") == 0)
    {
        *status = jobs_bg(jobs_parse_id(args[1]));
        return 1;
    }

    /* Builtins qui affichent du texte : ils s'executent DANS le shell, donc
       une redirection (say hi > f.txt, say hi < f.txt) changerait les
       descripteurs du shell lui-meme. On sauvegarde stdin ET stdout avec
       dup(), on redirige, on execute, puis on restaure avec dup2(). */
    int is_output_builtin =
        strcmp(args[0], "say") == 0 ||
        strcmp(args[0], "path") == 0 ||
        strcmp(args[0], "info") == 0 ||
        strcmp(args[0], "osinfo") == 0 ||
        strcmp(args[0], "past") == 0;

    if (is_output_builtin)
    {
        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);

        if (saved_stdin == -1 || saved_stdout == -1)
        {
            perror("ocsh: dup");

            if (saved_stdin != -1)
                close(saved_stdin);

            if (saved_stdout != -1)
                close(saved_stdout);

            *status = 1;
            return 1;
        }

        if (handle_redirection(args) == -1)
        {
            *status = 1;
        }
        else if (strcmp(args[0], "past") == 0)
        {
            display_custom_history();
            *status = 0;
        }
        else
        {
            *status = execute_builtin(args);
        }

        fflush(stdout);

        if (dup2(saved_stdin, STDIN_FILENO) == -1 ||
            dup2(saved_stdout, STDOUT_FILENO) == -1)
        {
            perror("ocsh: dup2");
            *status = 1;
        }

        close(saved_stdin);
        close(saved_stdout);
        return 1;
    }

    int builtin_result = execute_builtin(args);

    if (builtin_result != -1)
    {
        *status = builtin_result;
        return 1;
    }

    /* Affectation NOM=valeur */
    char *equal = strchr(args[0], '=');

    if (equal != NULL && args[1] == NULL &&
        is_valid_name(args[0], (size_t)(equal - args[0])))
    {
        *equal = '\0';

        if (setenv(args[0], equal + 1, 1) != 0)
        {
            perror("ocsh");
            *status = 1;
        }
        else
        {
            *status = 0;
        }

        return 1;
    }

    return 0;
}

int execute_line(const char *line)
{
    char substituted[MAX_COMMAND_LENGTH];
    char arithmetic[MAX_COMMAND_LENGTH];
    char expanded[MAX_COMMAND_LENGTH];
    char label[MAX_COMMAND_LENGTH];
    char single[MAX_COMMAND_LENGTH];
    char work[MAX_COMMAND_LENGTH];

    char *commands[MAX_COMMANDS];
    char *args[MAX_ARGS];

    expand_command_substitution((char *)line, substituted,
                                sizeof(substituted));

    expand_arithmetic(substituted, arithmetic, sizeof(arithmetic));

    expand_variables(arithmetic, expanded, sizeof(expanded),
                     g_last_status, g_last_background_pid);

    size_t length = strlen(expanded);

    while (length > 0 && isspace((unsigned char)expanded[length - 1]))
        expanded[--length] = '\0';

    int background = 0;

    if (length > 0 && expanded[length - 1] == '&')
    {
        background = 1;
        expanded[--length] = '\0';

        while (length > 0 && isspace((unsigned char)expanded[length - 1]))
            expanded[--length] = '\0';
    }

    const char *start = expanded;

    while (isspace((unsigned char)*start))
        start++;

    if (*start == '\0' || *start == '#')
        return g_last_status;

    strncpy(label, start, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';

    strncpy(work, start, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    int command_count = split_pipeline(work, commands, MAX_COMMANDS);

    if (command_count < 0)
    {
        fprintf(stderr, "ocsh: invalid command or empty pipeline\n");
        g_last_status = 2;
        return g_last_status;
    }

    if (command_count == 1 && !background)
    {
        strncpy(single, commands[0], sizeof(single) - 1);
        single[sizeof(single) - 1] = '\0';

        int i = tokenize(single, args, MAX_ARGS);

        if (i == 0)
            return g_last_status;

        int status = 0;

        if (run_builtin(args, &status))
        {
            g_last_status = status;
        }
        else
        {
            char *external_commands[] = { commands[0] };

            g_last_status = execute_pipeline(external_commands, 1, 0,
                                             label, NULL);
        }

        return g_last_status;
    }

    g_last_status = execute_pipeline(commands, command_count, background,
                                     label, &g_last_background_pid);

    return g_last_status;
}

int main(int argc, char *argv[])
{
    setup_jobs();

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    rl_attempted_completion_function = command_completion;

    if (argc > 1)
        return run_script(argv[1]);

    load_ocshrc();

    while (g_running)
    {
        jobs_notify_finished();

        char *command = readline("ocsh> ");

        if (command == NULL)
        {
            printf("\n");
            break;
        }

        if (command[0] != '\0')
        {
            add_custom_history(command);   /* pour la commande past      */
            add_history(command);          /* pour les fleches haut/bas */
            execute_line(command);
        }

        free(command);
    }

    jobs_cleanup();

    return g_last_status;
}