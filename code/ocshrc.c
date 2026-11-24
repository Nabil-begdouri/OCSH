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

    snprintf(path, sizeof(path), "%s/.ocshrc", home);

    FILE *file = fopen(path, "r");

    if (file == NULL)
        return;

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#')
            continue;

        execute_line(line);
    }

    fclose(file);
}
