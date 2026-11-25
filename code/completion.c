/*
 * completion.c — Completion avec la touche Tab (jalon 5), via readline.
 *
 * Premier mot de la ligne : on propose les commandes d'OCSH.
 * Mots suivants : completion de noms de fichiers (comportement de readline).
 */

#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include "completion.h"

/* Appelee plusieurs fois par readline : renvoie a chaque appel la commande
   suivante qui commence par 'text', puis NULL quand il n'y en a plus.
   state == 0 au premier appel : on repart du debut de la liste. */
char *command_generator(const char *text, int state)
{
    static int index;

    /* Liste des commandes proposees a la completion */
    static char *commands[] = {
        "go", "path", "info", "say", "osinfo",
        "quit", "past", "export", "list", "show",
        "copy", "move", "del", "newdir", "newfile",
        "jobs", "fg", "bg",
        NULL
    };

    if (!state)
        index = 0;

    while (commands[index])
    {
        char *command = commands[index++];

        if (strncmp(command, text, strlen(text)) == 0)
            /* readline libere lui-meme la chaine : il faut une copie allouee (strdup) */
            return strdup(command);
    }

    return NULL;
}

/* Fonction branchee sur readline dans main.c (rl_attempted_completion_function).
   start = position du mot a completer dans la ligne. */
char **command_completion(const char *text, int start, int end)
{
    (void)end;

    /* Apres le premier mot : completion de fichiers (comportement par defaut) */
    if (start > 0)
    {
        rl_attempted_completion_over = 0;
        return NULL;
    }

    return rl_completion_matches(text, command_generator);
}