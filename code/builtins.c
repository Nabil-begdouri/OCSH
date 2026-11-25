/*
 * builtins.c — Commandes internes d'OCSH et alias (jalon 2).
 *
 * Builtins (codes dans le shell)      Alias (traduits vers une commande Linux)
 *   go     = cd                         list    = ls      show    = cat
 *   path   = pwd                        copy    = cp      move    = mv
 *   say    = echo                       del     = rm      newdir  = mkdir
 *   info   = help                       newfile = touch
 *   osinfo = affiche le nom du cours
 * (quit, export, jobs, fg, bg et past sont geres dans main.c)
 */

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

/* Remplace le nom d'alias par la vraie commande, ex : list -> ls.
   La commande sera ensuite lancee normalement avec execvp. */
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

/* Execute un builtin dans le processus courant.
   Retourne son code de retour, ou -1 si args[0] n'est pas un builtin. */
int execute_builtin(char **args)
{
    if (args == NULL || args[0] == NULL)
        return 0;

    /* Alias (list, show...) : on traduit le nom ; aucun builtin ne
       correspondra ensuite, donc on retournera -1 et execvp fera le travail */
    translate_alias(args);

    /* go [dossier] : change le repertoire courant avec chdir().
       DOIT etre execute par le shell lui-meme : le repertoire courant est un
       attribut du processus, un chdir() dans un fils serait perdu.
       Sans argument (ou ~) : retour au dossier HOME. */
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

    /* path : affiche le repertoire courant (getcwd), comme pwd */
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

    /* info : aide, liste des commandes disponibles (comme help) */
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

    /* say : affiche ses arguments separes par un espace (comme echo) */
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

    /* osinfo : builtin demande par l'enonce */
    if (strcmp(args[0], "osinfo") == 0)
    {
        printf("Systèmes d’Exploitation -- UIR/ESIN\n");
        return 0;
    }

    /* Ce n'est pas un builtin : l'appelant lancera la commande avec execvp */
    return -1;
}

/* Version appelee dans un processus FILS, quand un builtin fait partie
   d'un pipeline ou est lance en arriere-plan (ex : say hi -> wc -c).
   Retourne -1 si ce n'est pas un builtin. */
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
