/*
 * history.c — Historique des commandes pour le builtin past (jalon 5).
 *
 * Les commandes sont gardees dans un tableau circulaire simple de
 * HISTORY_SIZE entrees (100). Quand il est plein, on supprime la plus
 * ancienne. L'historique n'est pas sauvegarde sur disque.
 * (Les fleches haut/bas utilisent l'historique de readline, voir main.c.)
 */

#include <stdio.h>
#include <string.h>
#include "history.h"

/* Tableau des commandes et nombre de cases utilisees.
   'static' : visibles uniquement dans ce fichier. */
static char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;

/* Ajoute une commande a la fin de l'historique. */
void add_custom_history(const char *command)
{
    /* Historique plein : on decale tout d'une case vers le haut
       pour effacer la plus ancienne commande (history[0]). */
    if (history_count == HISTORY_SIZE)
    {
        memmove(history[0], history[1],
                (HISTORY_SIZE - 1) * MAX_COMMAND_LENGTH);
        history_count--;
    }

    {
        /* Copie securisee : strncpy ne garantit pas le '\0' final,
           on l'ajoute donc nous-memes pour eviter un debordement. */
        strncpy(history[history_count], command, MAX_COMMAND_LENGTH - 1);
        history[history_count][MAX_COMMAND_LENGTH - 1] = '\0';
        history_count++;
    }
}

/* Affiche l'historique numerote (commande past). */
void display_custom_history(void)
{
    for (int i = 0; i < history_count; i++)
    {
        printf("%d  %s\n", i + 1, history[i]);
    }
}
