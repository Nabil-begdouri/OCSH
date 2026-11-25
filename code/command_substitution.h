/* command_substitution.h — Substitution $(...) et `...` (jalon 5). Code : command_substitution.c */

#ifndef COMMAND_SUBSTITUTION_H
#define COMMAND_SUBSTITUTION_H

#include <stddef.h>

/* Recopie input dans output en remplacant chaque $(...) ou `...` par la
   sortie de la commande. size = taille du tampon output. */
void expand_command_substitution(char *input, char *output, size_t size);

#endif
