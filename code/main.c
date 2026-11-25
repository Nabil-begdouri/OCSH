/*
 * main.c — Point d'entree d'OCSH : boucle REPL (jalon 1) et dispatch.
 *
 *   main()          : initialise le shell puis boucle :
 *                     readline -> historique -> execute_line()
 *   execute_line()  : applique les expansions puis execute la ligne
 *                     (builtin dans le shell, ou pipeline de processus)
 *   run_builtin()   : execute les commandes internes qui doivent tourner
 *                     DANS le shell (quit, export, go, jobs, fg, bg...)
 */

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

/* Variables globales du shell (declarees dans shell.h) :
   g_last_status         = code de retour de la derniere commande ($?)
   g_last_background_pid = PID du dernier job en arriere-plan ($!)
   g_running             = passe a 0 quand l'utilisateur tape quit */
int g_last_status = 0;
pid_t g_last_background_pid = 0;
int g_running = 1;

/* Verifie qu'un nom de variable est valide : lettre ou '_', puis lettres,
   chiffres ou '_' (ex : NOM, _x1). Utilise pour les affectations NOM=valeur. */
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

/* Execute args[0] s'il s'agit d'un builtin qui doit tourner dans le shell.
   Pourquoi dans le shell ? Un fils ne peut pas modifier son pere : un chdir()
   (go) ou un setenv() (export) fait dans un fils serait perdu a sa mort.
   Retourne 1 si c'etait un builtin (code de retour dans *status), 0 sinon. */
static int run_builtin(char **args, int *status)
{
    /* quit : arrete la boucle REPL (le shell se termine proprement) */
    if (strcmp(args[0], "quit") == 0)
    {
        printf("Goodbye!\n");
        g_running = 0;
        *status = g_last_status;
        return 1;
    }

    /* export NOM=valeur : modifie l'environnement du shell (herite par les fils) */
    if (strcmp(args[0], "export") == 0)
    {
        *status = execute_export(args);
        return 1;
    }

    /* Job control : jobs (liste), fg (premier plan), bg (relance en fond) */
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

    /* Autres builtins (go...) ; -1 si ce n'est pas un builtin */
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

/* Execute une ligne complete. Ordre des etapes :
    1. substitution $(...) et `...`
    2. arithmetique $((...))
    3. variables $VAR ${VAR} $? $!
    4. detection du '&' final (arriere-plan)
    5. decoupage par '->' en commandes
    6. builtin execute dans le shell, ou pipeline de processus
   Utilisee par la boucle interactive, le mode script et $(...). */
int execute_line(const char *line)
{
    char substituted[MAX_COMMAND_LENGTH];
    char arithmetic[MAX_COMMAND_LENGTH];
    char expanded[MAX_COMMAND_LENGTH];
    char label[MAX_COMMAND_LENGTH];
    char single[MAX_COMMAND_LENGTH];
    char work[MAX_COMMAND_LENGTH];

    /* commands = les commandes separees par '->' ; args = les mots d'une commande */
    char *commands[MAX_COMMANDS];
    char *args[MAX_ARGS];

    /* Etapes 1 a 3 : les expansions, chacune ecrit dans un nouveau tampon */
    expand_command_substitution((char *)line, substituted,
                                sizeof(substituted));

    expand_arithmetic(substituted, arithmetic, sizeof(arithmetic));

    expand_variables(arithmetic, expanded, sizeof(expanded),
                     g_last_status, g_last_background_pid);

    /* On retire les espaces en fin de ligne */
    size_t length = strlen(expanded);

    while (length > 0 && isspace((unsigned char)expanded[length - 1]))
        expanded[--length] = '\0';

    /* Etape 4 : un '&' en fin de ligne = execution en arriere-plan */
    int background = 0;

    if (length > 0 && expanded[length - 1] == '&')
    {
        background = 1;
        expanded[--length] = '\0';

        while (length > 0 && isspace((unsigned char)expanded[length - 1]))
            expanded[--length] = '\0';
    }

    /* Ligne vide ou commentaire (#) : rien a faire */
    const char *start = expanded;

    while (isspace((unsigned char)*start))
        start++;

    if (*start == '\0' || *start == '#')
        return g_last_status;

    /* label = texte de la commande (affiche par jobs) ; work = copie modifiable
       (split_pipeline ecrit des '\0' dedans) */
    strncpy(label, start, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';

    strncpy(work, start, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    /* Etape 5 : decoupage par '->' */
    int command_count = split_pipeline(work, commands, MAX_COMMANDS);

    if (command_count < 0)
    {
        fprintf(stderr, "ocsh: invalid command or empty pipeline\n");
        g_last_status = 2;
        return g_last_status;
    }

    /* Etape 6a : une seule commande au premier plan -> peut-etre un builtin */
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
            /* Pas un builtin : commande externe (fork + execvp) */
            char *external_commands[] = { commands[0] };

            g_last_status = execute_pipeline(external_commands, 1, 0,
                                             label, NULL);
        }

        return g_last_status;
    }

    /* Etape 6b : pipeline (ou commande en arriere-plan) : un fork par commande.
       Les builtins y sont executes dans le processus fils. */
    g_last_status = execute_pipeline(commands, command_count, background,
                                     label, &g_last_background_pid);

    return g_last_status;
}

/* Point d'entree : ./ocsh (interactif) ou ./ocsh script.ocsh (mode script) */
int main(int argc, char *argv[])
{
    /* Prepare le job control (groupe de processus, terminal, gestionnaire SIGCHLD) */
    setup_jobs();

    /* Le shell ignore Ctrl+C et Ctrl+\ : seules les commandes lancees les recoivent */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    /* Completion avec Tab (voir completion.c) */
    rl_attempted_completion_function = command_completion;

    /* Un argument : on execute le fichier comme un script, puis on quitte */
    if (argc > 1)
        return run_script(argv[1]);

    /* Mode interactif : on charge ~/.ocshrc (configuration de l'utilisateur) */
    load_ocshrc();

    /* ===== Boucle REPL : Read - Eval - Print - Loop ===== */
    while (g_running)
    {
        /* Affiche les jobs d'arriere-plan termines depuis le dernier prompt */
        jobs_notify_finished();

        /* readline affiche le prompt et lit une ligne (avec edition et historique) */
        char *command = readline("ocsh> ");

        /* NULL = fin de l'entree (Ctrl+D) : on quitte */
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

        /* readline alloue la ligne avec malloc : on la libere */
        free(command);
    }

    /* Avant de quitter : on libere la table des jobs */
    jobs_cleanup();

    return g_last_status;
}