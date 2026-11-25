/* parser.h — Analyse d'une ligne de commande (jalons 1 et 3). Code : parser.c */

#ifndef PARSER_H
#define PARSER_H

/* Nombre maximal de commandes dans un pipeline cmd1 -> cmd2 -> ... */
#define MAX_COMMANDS 16

/* Coupe la ligne aux '->' (hors guillemets) : commands[i] = commande i.
   Retourne le nombre de commandes, ou -1 si erreur (ex : "ls ->"). */
int split_pipeline(char *input, char *commands[], int max_commands);

/* Decoupe une commande en arguments (en place). Gere les guillemets
   simples/doubles (retires du resultat) et l'echappement \\.
   Retourne le nombre d'arguments ; args[n] = NULL. */
int tokenize(char *line, char *args[], int max_args);

#endif
