/* script.h — Mode script et calcul $((...)) (jalon 6). Code : script.c */

#ifndef SCRIPT_H
#define SCRIPT_H

#include <stddef.h>

/* Execute un fichier script (ocsh script.ocsh) : lignes, if / for / while.
   Retourne le code de retour du script. */
int run_script(const char *path);

/* Remplace $((expression)) par sa valeur (entiers, + - * / % et parentheses). */
void expand_arithmetic(const char *input, char *output, size_t size);

#endif
