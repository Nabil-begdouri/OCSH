#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "history.h"
#include "variables.h"

/* Table des alias ocsh -> commande Linux (une seule copie dans le projet) */
static const char *aliases[][2] = {
    { "list",    "ls"    },
    { "show",    "cat"   },
    { "copy",    "cp"    },
    { "move",    "mv"    },
    { "del",     "rm"    },
    { "newdir",  "mkdir" },
    { "newfile", "touch" },
};

void translate_alias(char **args)
{
    if (args == NULL || args[0] == NULL)
        return;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
    {
        if (strcmp(args[0], aliases[i][0]) == 0)
        {
            args[0] = (char *)aliases[i][1];
            return;
        }
    }
}

int execute_builtin(char **args)
{
    if (args == NULL || args[0] == NULL)
        return 0;

    /* Alias (list, show...) : on traduit le nom ; aucun builtin ne
       correspondra ensuite, donc on retournera -1 et execvp fera le travail */
    translate_alias(args);

    if (strcmp(args[0], "go") == 0)
    {
        char *home = getenv("HOME");

        if (args[1] == NULL || strcmp(args[1], "~") == 0)
        {
            if (home == NULL || chdir(home) != 0)
            {
                perror("cd");
                return 1;
            }
        }
        else
        {
            if (chdir(args[1]) != 0)
            {
                perror("cd");
                return 1;
            }
        }

        return 0;
    }

    if (strcmp(args[0], "path") == 0)
    {
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("pwd");
            return 1;
        }

        printf("%s\n", cwd);
        return 0;
    }

    if (strcmp(args[0], "info") == 0)
    {
        printf("OCSH - Commandes disponibles\n");
        printf("  go [dir]   : changer de repertoire (cd)\n");
        printf("  path       : repertoire courant (pwd)\n");
        printf("  say        : afficher du texte (echo)\n");
        printf("  list       : lister les fichiers (ls)\n");
        printf("  show       : afficher un fichier (cat)\n");
        printf("  copy       : copier (cp)\n");
        printf("  move       : deplacer/renommer (mv)\n");
        printf("  del        : supprimer (rm)\n");
        printf("  newdir     : creer un dossier (mkdir)\n");
        printf("  newfile    : creer un fichier (touch)\n");
        printf("  past       : historique (history)\n");
        printf("  info       : cette aide (help)\n");
        printf("  quit       : quitter (exit)\n");
        printf("  osinfo     : informations du cours\n");
        printf("  export N=V : definir une variable d'environnement\n");
        printf("  jobs/fg/bg : gestion des jobs\n");
        printf("  cmd1 -> cmd2 : pipe ; > >> < : redirections ; & : arriere-plan\n");

        return 0;
    }

    if (strcmp(args[0], "say") == 0)
    {
        for (int i = 1; args[i] != NULL; i++)
        {
            printf("%s", args[i]);

            if (args[i + 1] != NULL)
                printf(" ");
        }

        printf("\n");
        return 0;
    }

    if (strcmp(args[0], "osinfo") == 0)
    {
        printf("Systèmes d’Exploitation -- UIR/ESIN\n");
        return 0;
    }

    return -1;
}

int execute_builtin_in_child(char **args)
{
    if (args == NULL || args[0] == NULL)
        return -1;

    /* past : le fils a herite d'une copie de l'historique du shell */
    if (strcmp(args[0], "past") == 0)
    {
        display_custom_history();
        return 0;
    }

    /* export / quit / jobs dans un fils : sans effet sur le shell
       (comme dans bash, ou "export X=1 | cat" ne change rien). */
    if (strcmp(args[0], "export") == 0)
        return execute_export(args);

    if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "jobs") == 0
        || strcmp(args[0], "fg") == 0 || strcmp(args[0], "bg") == 0)
        return 0;

    return execute_builtin(args);
}
