/* pipes.h — Execution des commandes et des pipelines (jalons 1 et 3). Code : pipes.c */

#ifndef PIPES_H
#define PIPES_H

#include <sys/types.h>

/* Execute un pipeline de 'command_count' commandes (1 = commande simple).
   - background = 0 : attend la fin, retourne le code de la derniere commande.
   - background = 1 : lance en arriere-plan, enregistre le job (texte 'label'),
                      met le PID de la derniere commande dans *last_pid. */
int execute_pipeline(char *commands[], int command_count,
                     int background, const char *label, pid_t *last_pid);

#endif
