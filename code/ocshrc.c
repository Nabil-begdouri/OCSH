/*
 * ocshrc.c — Fichier de configuration ~/.ocshrc (jalon 5).
 *
 * Equivalent du ~/.bashrc : au demarrage du mode interactif, chaque ligne du
 * fichier est executee (ex : export EDITOR=nano). Lignes vides et # ignorees.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ocshrc.h"
#include "shell.h"

/* Charge ~/.ocshrc au demarrage : chaque ligne est executee comme si
   l'utilisateur l'avait tapee (builtins, export, alias, etc.). */
void load_ocshrc(void)
{
    const char *home = getenv("HOME");
    char path[512];
    char line[MAX_COMMAND_LENGTH];

    if (home == NULL)
        return;

    /* Chemin complet du fichier : $HOME/.ocshrc */
    snprintf(path, sizeof(path), "%s/.ocshrc", home);

    FILE *file = fopen(path, "r");

    /* Pas de fichier .ocshrc : ce n'est pas une erreur, on ne fait rien */
    if (file == NULL)
        return;

    while (fgets(line, sizeof(line), file))
    {
        /* Retire le '\n' laisse par fgets */
        line[strcspn(line, "\n")] = '\0';

        /* Ligne vide ou commentaire : on passe */
        if (line[0] == '\0' || line[0] == '#')
            continue;

        execute_line(line);
    }

    fclose(file);
}
