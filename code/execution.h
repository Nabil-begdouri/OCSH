/* execution.h — Execution d'une commande simple, version du jalon 1. Code : execution.c */

#ifndef EXECUTION_H
#define EXECUTION_H

/* fork + execvp + waitpid pour une seule commande. Retourne son code de retour.
   (Remplacee dans le shell final par execute_pipeline, voir pipes.h) */
int execute_command(char **args);

#endif
