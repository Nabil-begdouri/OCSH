/* variables.h — Variables d'environnement et expansion (jalon 4). Code : variables.c */

#ifndef VARIABLES_H
#define VARIABLES_H

#include <stddef.h>
#include <sys/types.h>

/* Remplace $VAR, ${VAR}, $? et $! dans input ; resultat dans output */
void expand_variables(char *input, char *output, size_t size,
                      int last_status, pid_t last_background_pid);

/* Builtin export NOM=valeur (setenv) ; retourne 0 si OK, 1 si erreur */
int execute_export(char *args[]);

#endif
