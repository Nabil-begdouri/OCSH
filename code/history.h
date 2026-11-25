/* history.h — Historique des commandes pour le builtin past (jalon 5). Code : history.c */

#ifndef HISTORY_H
#define HISTORY_H

/* Taille maximale d'une commande et nombre de commandes gardees */
#define MAX_COMMAND_LENGTH 1024
#define HISTORY_SIZE 100

/* Ajoute une commande a l'historique (la plus ancienne est oubliee si plein) */
void add_custom_history(const char *command);

/* Affiche l'historique numerote (commande past) */
void display_custom_history(void);

#endif
