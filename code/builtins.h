/* builtins.h — Commandes internes d'OCSH et alias (jalon 2). Code : builtins.c */

#ifndef BUILTINS_H
#define BUILTINS_H

/* Remplace un alias ocsh par la vraie commande Linux
   (list -> ls, show -> cat, ...). Table unique, utilisee partout. */
void translate_alias(char **args);

/* Execute un builtin (go, path, say, info, osinfo) dans le processus courant.
   Retourne son code de retour, ou -1 si args[0] n'est pas un builtin
   (dans ce cas les alias ont ete traduits et il faut faire execvp). */
int execute_builtin(char **args);

/* Version pour un processus fils (pipeline ou arriere-plan) : gere aussi
   past, export, jobs... Retourne -1 si ce n'est pas un builtin. */
int execute_builtin_in_child(char **args);

#endif
