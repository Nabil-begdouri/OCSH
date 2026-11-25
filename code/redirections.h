/* redirections.h — Redirections > >> < avec open + dup2 (jalon 2). Code : redirections.c */

#ifndef REDIRECTIONS_H
#define REDIRECTIONS_H

/* Applique toutes les redirections de args et les retire de args.
   Retourne le nombre de redirections, ou -1 en cas d'erreur. */
int handle_redirection(char **args);

#endif
