#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "pipes.h"
#include "jobs.h"
#include "script.h"
#include "shell.h"

/* Etat global du shell (declare dans shell.h) */
int   g_last_status = 0;
pid_t g_last_background_pid = 0;
int   g_running = 1;

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

/* NOM valide : lettre ou '_' puis lettres, chiffres, '_' */
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

/* Commandes executees par le shell lui-meme (pas de fork).
   Retourne 1 si 'args[0]' etait un builtin (code de retour dans *status).
   C'est ICI que les autres jalons ajoutent leurs builtins (go, path, say...). */
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
        if (args[1] == NULL || strchr(args[1], '=') == NULL)
        {
            printf("Usage: export NOM=valeur\n");
            *status = 1;
        }
        else
        {
            char *equal = strchr(args[1], '=');
            *equal = '\0';

            if (setenv(args[1], equal + 1, 1) != 0)
            {
                perror("export");
                *status = 1;
            }
            else
            {
                *status = 0;
            }
        }

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

    /* Affectation simple : NOM=valeur (utile dans les scripts) */
    char *equal = strchr(args[0], '=');

    if (equal != NULL && args[1] == NULL
        && is_valid_name(args[0], (size_t)(equal - args[0])))
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
    char arithmetic[MAX_COMMAND_LENGTH];
    char expanded[MAX_COMMAND_LENGTH];
    char label[MAX_COMMAND_LENGTH];
    char single[MAX_COMMAND_LENGTH];
    char *commands[MAX_COMMANDS];
    char *args[MAX_ARGS];

    /* 1. Expansions : $((...)) puis $VAR, ${VAR}, $?, $! */
    expand_arithmetic(line, arithmetic, sizeof(arithmetic));
    expand_variables(arithmetic, expanded, sizeof(expanded),
                     g_last_status, g_last_background_pid);

    /* 2. Espaces de fin, puis '&' final = arriere-plan */
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

    /* Ligne vide ou commentaire : rien a faire */
    if (*start == '\0' || *start == '#')
        return g_last_status;

    /* Texte affiche par le builtin jobs (copie avant que le pipeline soit decoupe) */
    strncpy(label, start, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';

    /* 3. Decoupage selon l'operateur -> */
    char work[MAX_COMMAND_LENGTH];
    strncpy(work, start, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    int command_count = split_pipeline(work, commands, MAX_COMMANDS);

    if (command_count < 0)
    {
        fprintf(stderr, "ocsh: invalid command or empty pipeline\n");
        g_last_status = 2;
        return g_last_status;
    }

    /* 4. Commande simple au premier plan : builtin ou fork/exec */
    if (command_count == 1 && !background)
    {
        strncpy(single, commands[0], sizeof(single) - 1);
        single[sizeof(single) - 1] = '\0';

        int i = 0;
        char *token = strtok(single, " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i++] = token;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        if (i == 0)
            return g_last_status;

        int status = 0;

        if (run_builtin(args, &status))
            g_last_status = status;
        else
            /* Commande externe : meme chemin que les pipelines, pour que
               Ctrl+C / Ctrl+Z et fg / bg marchent aussi sur une commande seule. */
            g_last_status = execute_pipeline(commands, 1, 0, label, NULL);

        return g_last_status;
    }

    /* 5. Pipeline (-> ) et/ou arriere-plan (&) */
    g_last_status = execute_pipeline(commands, command_count, background,
                                     label, &g_last_background_pid);

    return g_last_status;
}

int main(int argc, char *argv[])
{
    setup_jobs();

    /* Ctrl+C ne doit pas tuer le shell, seulement la commande en cours. */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);   /* Ctrl+\ ne tue pas le shell */

    /* Mode script : ./ocsh script.ocsh */
    if (argc > 1)
        return run_script(argv[1]);

    char command[MAX_COMMAND_LENGTH];

    while (g_running)
    {
        jobs_notify_finished();

        printf("ocsh> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        execute_line(command);
    }

    jobs_cleanup();   /* reveille et termine les jobs encore suspendus */

    return g_last_status;
}
