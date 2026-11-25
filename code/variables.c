/*
 * variables.c — Variables d'environnement et expansion (jalon 4).
 *
 *   $VAR, ${VAR} : valeur de la variable (getenv) ; vide si elle n'existe pas
 *   $?           : code de retour de la derniere commande
 *   $!           : PID du dernier job lance en arriere-plan
 *   export N=V   : cree/modifie une variable (setenv), heritee par les fils
 * Comme dans Bash, rien n'est remplace entre apostrophes : '$HOME' reste $HOME.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "variables.h"

/* Recopie input dans output en remplacant les $... par leur valeur.
   size = taille de output : chaque copie est limitee a la place restante
   (size - 1 - j) pour ne jamais deborder du tampon. */
void expand_variables(char *input, char *output, size_t size,
                      int last_status, pid_t last_background_pid)
{
    if (input == NULL || output == NULL || size == 0)
        return;

    /* i = position de lecture dans input, j = position d'ecriture dans output */
    size_t i = 0;
    size_t j = 0;

    int in_single = 0;   /* dans '...' : pas d'expansion (comme Bash) */
    int in_double = 0;   /* dans "..." : une apostrophe ne compte pas */

    while (input[i] != '\0' && j < size - 1)
    {
        if (input[i] == '"' && !in_single)
            in_double = !in_double;
        else if (input[i] == '\'' && !in_double)
            in_single = !in_single;

        /* Caractere normal (ou entre apostrophes) : recopie tel quel */
        if (in_single || input[i] != '$')
        {
            output[j++] = input[i++];
            continue;
        }

        /* On saute le '$' */
        i++;

        /* $? : code de retour de la derniere commande */
        if (input[i] == '?')
        {
            char value[16];
            snprintf(value, sizeof(value), "%d", last_status);

            size_t length = strlen(value);
            if (length > size - 1 - j)
                length = size - 1 - j;

            memcpy(output + j, value, length);
            j += length;
            i++;
            continue;
        }

        /* $! : PID du dernier processus en arriere-plan */
        if (input[i] == '!')
        {
            char value[32];
            snprintf(value, sizeof(value), "%ld",
                     (long)last_background_pid);

            size_t length = strlen(value);
            if (length > size - 1 - j)
                length = size - 1 - j;

            memcpy(output + j, value, length);
            j += length;
            i++;
            continue;
        }

        /* Sinon, on lit le nom de la variable */
        char variable[128];
        size_t k = 0;

        /* Forme ${NOM} : le nom va jusqu'a l'accolade fermante */
        if (input[i] == '{')
        {
            i++;

            while (input[i] != '\0' && input[i] != '}')
            {
                if (k >= sizeof(variable) - 1)
                    break;

                variable[k++] = input[i++];
            }

            if (input[i] == '}')
                i++;
        }
        else
        {
            /* Forme $NOM : lettres, chiffres et '_' */
            while (isalnum((unsigned char)input[i])
                   || input[i] == '_')
            {
                if (k >= sizeof(variable) - 1)
                    break;

                variable[k++] = input[i++];
            }
        }

        variable[k] = '\0';

        if (k > 0)
        {
            /* Valeur dans l'environnement du shell ; variable inconnue = texte vide */
            char *value = getenv(variable);

            if (value != NULL)
            {
                size_t length = strlen(value);

                if (length > size - 1 - j)
                    length = size - 1 - j;

                memcpy(output + j, value, length);
                j += length;
            }
        }
    }

    output[j] = '\0';
}

/* export NOM=valeur : verifie le nom puis appelle setenv().
   La variable est ensuite heritee par tous les processus fils. */
int execute_export(char *args[])
{
    if (args == NULL || args[1] == NULL ||
        args[2] != NULL || strchr(args[1], '=') == NULL)
    {
        printf("Usage: export NOM=valeur\n");
        return 1;
    }

    char *equal = strchr(args[1], '=');

    if (equal == args[1])
    {
        printf("export: invalid variable name\n");
        return 1;
    }

    /* Nom valide : lettres, chiffres, '_' et ne commence pas par un chiffre */
    for (char *p = args[1]; p < equal; p++)
    {
        if ((!isalnum((unsigned char)*p) && *p != '_') ||
            (p == args[1] && isdigit((unsigned char)*p)))
        {
            printf("export: invalid variable name\n");
            return 1;
        }
    }

    /* On coupe la chaine au '=' : args[1] = nom, equal + 1 = valeur */
    *equal = '\0';

    if (setenv(args[1], equal + 1, 1) != 0)
    {
        perror("export");
        return 1;
    }

    return 0;
}
