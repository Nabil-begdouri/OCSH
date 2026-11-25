/*
 * parser.c — Analyse d'une ligne de commande (jalons 1 et 3).
 *
 *   split_pipeline() : decoupe la ligne aux operateurs '->'
 *                      "show f.txt -> wc -l"  =>  "show f.txt" | "wc -l"
 *   tokenize()       : decoupe une commande en mots (argv pour execvp),
 *                      en respectant les guillemets "..." '...' et le \
 *                      'say "a   b" c'  =>  say | a   b | c
 */

#include <ctype.h>
#include <string.h>

#include "parser.h"

/* Decoupe input aux '->' (hors guillemets). Travaille EN PLACE : des '\0'
   sont ecrits dans input, et commands[] pointe vers chaque morceau.
   Retourne le nombre de commandes, ou -1 si erreur (guillemet non ferme,
   commande vide comme "ls ->", ou trop de commandes). */
int split_pipeline(char *input, char *commands[], int max_commands)
{
    if (input == NULL || commands == NULL || max_commands <= 0)
        return -1;

    int count = 0;
    char *start = input;
    /* Guillemet ouvert en cours ('\0' = aucun) : un '->' entre guillemets
       n'est pas un pipe */
    char quote = '\0';

    for (char *p = input; ; p++)
    {
        /* Fin de la ligne : on enregistre la derniere commande */
        if (*p == '\0')
        {
            if (quote != '\0')
                return -1;

            /* On retire les espaces au debut et a la fin du morceau */
            char *end = p;

            while (isspace((unsigned char)*start))
                start++;

            while (end > start && isspace((unsigned char)*(end - 1)))
                end--;

            *end = '\0';

            if (*start == '\0' || count >= max_commands)
                return -1;

            commands[count++] = start;
            break;
        }

        /* Ouverture ou fermeture d'un guillemet */
        if ((*p == '"' || *p == '\'') &&
            (quote == '\0' || quote == *p))
        {
            if (quote == '\0')
                quote = *p;
            else
                quote = '\0';

            continue;
        }

        /* Operateur '->' hors guillemets : fin de la commande courante */
        if (quote == '\0' && p[0] == '-' && p[1] == '>')
        {
            *p = '\0';

            char *end = p;

            while (isspace((unsigned char)*start))
                start++;

            while (end > start && isspace((unsigned char)*(end - 1)))
                end--;

            *end = '\0';

            if (*start == '\0' || count >= max_commands)
                return -1;

            commands[count++] = start;

            /* On saute le '>' ; la commande suivante commence juste apres */
            p++;
            start = p + 1;
        }
    }

    return count;
}

/* Decoupe line en mots dans args[] (termine par NULL, comme attend execvp).
   Les guillemets sont retires et leur contenu forme un seul mot.
   Travaille en place avec deux pointeurs : r lit, w ecrit.
   Retourne le nombre de mots. */
int tokenize(char *line, char *args[], int max_args)
{
    int count = 0;
    char *r = line;   /* lecture  */
    char *w = line;   /* ecriture (les quotes sont supprimees en place) */

    while (*r != '\0')
    {
        /* Saute les espaces entre deux mots */
        while (*r == ' ' || *r == '\t')
            r++;

        if (*r == '\0')
            break;

        /* On garde une case pour le NULL final */
        if (count >= max_args - 1)
            break;

        /* Debut d'un nouveau mot */
        args[count++] = w;
        char quote = '\0';

        /* Lit le mot jusqu'a un espace (sauf si on est entre guillemets) */
        while (*r != '\0' && (quote != '\0' || (*r != ' ' && *r != '\t')))
        {
            if (quote != '\0')
            {
                if (*r == quote)
                    quote = '\0';
                else
                    *w++ = *r;
                r++;
            }
            else if (*r == '"' || *r == '\'')
            {
                quote = *r++;
            }
            else if (*r == '\\' && r[1] != '\0')
            {
                /* Antislash : le caractere suivant est pris tel quel (ex : \") */
                r++;
                *w++ = *r++;
            }
            else
            {
                *w++ = *r++;
            }
        }

        int at_end = (*r == '\0');
        /* Termine le mot */
        *w++ = '\0';

        if (!at_end)
            r++;
    }

    args[count] = NULL;
    return count;
}
