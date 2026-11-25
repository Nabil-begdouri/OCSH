#ifndef PARSER_H
#define PARSER_H

#define MAX_COMMANDS 16

int split_pipeline(char *input, char *commands[], int max_commands);

/* Decoupe une commande en arguments (en place). Gere les guillemets
   simples/doubles (retires du resultat) et l'echappement \\.
   Retourne le nombre d'arguments ; args[n] = NULL. */
int tokenize(char *line, char *args[], int max_args);

#endif