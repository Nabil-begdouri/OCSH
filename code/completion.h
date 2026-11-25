/* completion.h — Completion avec la touche Tab via readline (jalon 5). Code : completion.c */

#ifndef COMPLETION_H
#define COMPLETION_H

/* Renvoie, appel apres appel, les commandes OCSH qui commencent par text */
char *command_generator(const char *text, int state);

/* Fonction branchee sur readline : complete le 1er mot avec les commandes
   OCSH, les mots suivants avec les noms de fichiers */
char **command_completion(const char *text, int start, int end);

#endif
