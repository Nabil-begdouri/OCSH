#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

/* Etat global du shell (defini dans main.c) */
extern int   g_last_status;          /* valeur de $?                    */
extern pid_t g_last_background_pid;  /* valeur de $!                    */
extern int   g_running;              /* passe a 0 apres la commande quit */

/* Execute UNE ligne de commande (utilise par le REPL et par les scripts).
   Retourne le code de retour de la ligne. */
int execute_line(const char *line);

/* Expansion de $VAR, ${VAR}, $? et $! (definie dans main.c). */
void expand_variables(char *input, char *output, size_t size,
                      int last_status, pid_t last_background_pid);

#endif
