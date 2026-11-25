/*
 * script.c — Mode script : ./ocsh fichier.ocsh (jalon 6).
 *
 * Le fichier est lu en entier en memoire (une case par ligne), puis execute
 * bloc par bloc. Chaque ligne simple est passee a execute_line() : un script
 * peut donc utiliser tout ce que le shell sait faire (builtins, pipes, $VAR...).
 *
 * Structures reconnues (une par ligne, comme en Bash) :
 *   if CONDITION ; then ... [else ...] fi
 *   for VAR in mot1 mot2 ... ; do ... done
 *   while CONDITION ; do ... done
 *   break, continue, commentaires (#) et $((calcul))
 *
 * Une CONDITION est une commande : elle est vraie si elle retourne 0.
 * Plan du fichier : 1. arithmetique  2. lecture  3. outils  4. execution
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "script.h"
#include "shell.h"

/* ======================================================================
 *  1. Arithmetique : $((i + 1))
 *     Petit evaluateur a descente recursive :
 *        expr    = terme { (+|-) terme }
 *        terme   = facteur { (*|/|%) facteur }
 *        facteur = nombre | variable | ( expr ) | -facteur
 * ====================================================================== */

typedef struct
{
    const char *p;   /* position courante dans l'expression */
    int error;
} Calc;

/* Declaration anticipee : calc_factor appelle calc_expr pour les parentheses */
static long calc_expr(Calc *c);

/* Saute les espaces dans l'expression */
static void calc_skip(Calc *c)
{
    while (isspace((unsigned char)*c->p))
        c->p++;
}

/* facteur : un nombre, une variable, (expression) ou -facteur.
   C'est le niveau de priorite le plus fort. */
static long calc_factor(Calc *c)
{
    calc_skip(c);

    if (*c->p == '(')
    {
        c->p++;
        long value = calc_expr(c);
        calc_skip(c);

        if (*c->p == ')')
            c->p++;
        else
            c->error = 1;

        return value;
    }

    if (*c->p == '-')
    {
        c->p++;
        return -calc_factor(c);
    }

    if (*c->p == '+')
    {
        c->p++;
        return calc_factor(c);
    }

    /* $i et i sont acceptes : on ignore le '$' devant un nom */
    if (*c->p == '$')
        c->p++;

    if (isdigit((unsigned char)*c->p))
    {
        char *end;
        long value = strtol(c->p, &end, 10);
        c->p = end;
        return value;
    }

    if (isalpha((unsigned char)*c->p) || *c->p == '_')
    {
        char name[128];
        size_t k = 0;

        while ((isalnum((unsigned char)*c->p) || *c->p == '_')
               && k < sizeof(name) - 1)
            name[k++] = *c->p++;

        name[k] = '\0';

        /* Variable : sa valeur est convertie en nombre (0 si elle n'existe pas) */
        const char *value = getenv(name);
        return value != NULL ? strtol(value, NULL, 10) : 0;
    }

    c->error = 1;
    return 0;
}

/* terme : facteurs relies par * / %. Calcule AVANT + et -,
   ce qui donne la bonne priorite : 2+3*4 = 2+(3*4) = 14. */
static long calc_term(Calc *c)
{
    long value = calc_factor(c);

    for (;;)
    {
        calc_skip(c);
        char op = *c->p;

        if (op != '*' && op != '/' && op != '%')
            return value;

        c->p++;
        long right = calc_factor(c);

        if (op == '*')
        {
            value *= right;
        }
        else if (right == 0)
        {
            c->error = 1;   /* division par zero */
            return 0;
        }
        else if (op == '/')
        {
            value /= right;
        }
        else
        {
            value %= right;
        }
    }
}

/* expression : termes relies par + et - (priorite la plus faible) */
static long calc_expr(Calc *c)
{
    long value = calc_term(c);

    for (;;)
    {
        calc_skip(c);
        char op = *c->p;

        if (op != '+' && op != '-')
            return value;

        c->p++;
        long right = calc_term(c);

        value = (op == '+') ? value + right : value - right;
    }
}

/* Remplace chaque $((expression)) de input par son resultat dans output.
   Exemple : "say $((2+3))" devient "say 5". Appelee par execute_line(). */
void expand_arithmetic(const char *input, char *output, size_t size)
{
    size_t i = 0;
    size_t j = 0;

    if (size == 0)
        return;

    while (input[i] != '\0' && j < size - 1)
    {
        /* Debut d'un calcul : $(( */
        if (input[i] == '$' && input[i + 1] == '(' && input[i + 2] == '(')
        {
            const char *start = input + i + 3;
            const char *p = start;
            int depth = 0;

            /* cherche le "))" qui ferme $(( */
            while (*p != '\0')
            {
                if (*p == '(')
                {
                    depth++;
                }
                else if (*p == ')')
                {
                    if (depth == 0)
                        break;

                    depth--;
                }

                p++;
            }

            if (p[0] == ')' && p[1] == ')')
            {
                char expression[256];
                size_t length = (size_t)(p - start);

                if (length >= sizeof(expression))
                    length = sizeof(expression) - 1;

                memcpy(expression, start, length);
                expression[length] = '\0';

                /* Calcul de l'expression ; erreur si syntaxe invalide ou division par zero */
                Calc calc = { expression, 0 };
                long value = calc_expr(&calc);
                calc_skip(&calc);

                if (calc.error || *calc.p != '\0')
                {
                    fprintf(stderr, "ocsh: arithmetic error: %s\n",
                            expression);
                    value = 0;
                }

                /* Le resultat est ecrit en texte a la place de $((...)) */
                char number[32];
                snprintf(number, sizeof(number), "%ld", value);

                size_t n = strlen(number);
                if (n > size - 1 - j)
                    n = size - 1 - j;

                memcpy(output + j, number, n);
                j += n;

                i = (size_t)(p + 2 - input);
                continue;
            }
        }

        /* Caractere normal : recopie tel quel */
        output[j++] = input[i++];
    }

    output[j] = '\0';
}

/* ======================================================================
 *  2. Lecture du script
 * ====================================================================== */

typedef struct
{
    char **lines;
    int count;
} Script;

/* Etat du script en cours d'execution */
static const char *script_name = "script";
static int script_failed = 0;   /* erreur de syntaxe : on arrete tout */
static int loop_signal = 0;     /* 0 = rien, 1 = break, 2 = continue  */
static int loop_depth = 0;      /* profondeur de boucles en cours      */

/* Libere toutes les lignes du script */
static void free_script(Script *script)
{
    for (int i = 0; i < script->count; i++)
        free(script->lines[i]);

    free(script->lines);
    script->lines = NULL;
    script->count = 0;
}

/* Lit tout le fichier en memoire : script->lines[i] = ligne i.
   getline() alloue chaque ligne ; le tableau s'agrandit avec realloc
   (taille doublee a chaque fois). Retourne 0, ou -1 en cas d'erreur. */
static int read_script(const char *path, Script *script)
{
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return -1;

    script->lines = NULL;
    script->count = 0;

    int capacity = 0;
    char *buffer = NULL;
    size_t buffer_size = 0;
    ssize_t length;

    while ((length = getline(&buffer, &buffer_size, file)) != -1)
    {
        /* Retire les fins de ligne \n (Linux) et \r (fichiers Windows) */
        while (length > 0
               && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
            buffer[--length] = '\0';

        /* Tableau plein : on double sa taille */
        if (script->count == capacity)
        {
            capacity = capacity ? capacity * 2 : 64;

            char **bigger = realloc(script->lines,
                                    (size_t)capacity * sizeof(char *));

            if (bigger == NULL)
            {
                free(buffer);
                fclose(file);
                free_script(script);
                return -1;
            }

            script->lines = bigger;
        }

        script->lines[script->count] = strdup(buffer);

        if (script->lines[script->count] == NULL)
        {
            free(buffer);
            fclose(file);
            free_script(script);
            return -1;
        }

        script->count++;
    }

    free(buffer);
    fclose(file);
    return 0;
}

/* ======================================================================
 *  3. Petits outils d'analyse
 * ====================================================================== */

/* Saute les espaces au debut d'une chaine */
static const char *skip_ws(const char *s)
{
    while (isspace((unsigned char)*s))
        s++;

    return s;
}

/* Supprime les espaces a la fin d'une chaine */
static void rtrim(char *s)
{
    size_t n = strlen(s);

    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

/* La ligne commence-t-elle par le mot 'word' (mot entier) ? */
static int word_is(const char *line, const char *word)
{
    size_t n = strlen(word);

    return strncmp(line, word, n) == 0
           && (line[n] == '\0' || isspace((unsigned char)line[n])
               || line[n] == ';');
}

/* Nom de variable valide pour for : lettre ou '_', puis lettres, chiffres, '_' */
static int is_valid_name(const char *name)
{
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_'))
        return 0;

    for (size_t i = 1; name[i] != '\0'; i++)
    {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
            return 0;
    }

    return 1;
}

/* Affiche une erreur de syntaxe avec le numero de ligne, puis arrete
   le script (script_failed) avec le code 2, comme Bash. */
static void syntax_error(int line_index, const char *message)
{
    fprintf(stderr, "ocsh: %s: line %d: %s\n",
            script_name, line_index + 1, message);

    g_last_status = 2;
    script_failed = 1;
}

/* Retire un suffixe "; kw" ou " kw" de la fin de 's'. Retourne 1 si retire. */
static int strip_suffix(char *s, const char *kw)
{
    rtrim(s);

    size_t n = strlen(s);
    size_t k = strlen(kw);

    if (n < k || strcmp(s + n - k, kw) != 0)
        return 0;

    if (n > k && !(isspace((unsigned char)s[n - k - 1]) || s[n - k - 1] == ';'))
        return 0;

    s[n - k] = '\0';
    rtrim(s);

    n = strlen(s);
    if (n > 0 && s[n - 1] == ';')
        s[n - 1] = '\0';

    rtrim(s);
    return 1;
}

/* Prepare le texte d'un en-tete (ce qui suit "if", "for" ou "while").
   Retourne 1 si "then"/"do" etait sur la meme ligne (ex: "if cmd; then"). */
static int parse_header(const char *line, size_t keyword_length,
                        const char *kw, char *out, size_t size)
{
    strncpy(out, skip_ws(line + keyword_length), size - 1);
    out[size - 1] = '\0';

    int inline_keyword = strip_suffix(out, kw);

    if (!inline_keyword)
    {
        rtrim(out);

        size_t n = strlen(out);
        if (n > 0 && out[n - 1] == ';')
            out[n - 1] = '\0';

        rtrim(out);
    }

    return inline_keyword;
}

/* Index de la premiere ligne du corps (apres "then"/"do"), ou -1 s'il manque. */
static int body_start(Script *script, int header, int inline_keyword,
                      const char *kw, int limit)
{
    if (inline_keyword)
        return header + 1;

    for (int i = header + 1; i < limit; i++)
    {
        const char *line = skip_ws(script->lines[i]);

        if (*line == '\0' || *line == '#')
            continue;

        if (word_is(line, kw))
            return i + 1;

        return -1;
    }

    return -1;
}

/* Trouve la ligne "fi" / "done" qui ferme le bloc ouvert a la ligne 'open'
   (en tenant compte des blocs imbriques). Pour un "if", donne aussi
   l'index du "else" de meme niveau (ou -1). Retourne -1 si non ferme. */
static int find_end(Script *script, int open, const char *closer,
                    int *else_index)
{
    int depth = 0;

    if (else_index != NULL)
        *else_index = -1;

    for (int i = open + 1; i < script->count; i++)
    {
        const char *line = skip_ws(script->lines[i]);

        if (word_is(line, "if") || word_is(line, "for")
            || word_is(line, "while"))
        {
            depth++;
        }
        else if (word_is(line, "fi") || word_is(line, "done"))
        {
            if (depth == 0)
                return word_is(line, closer) ? i : -1;

            depth--;
        }
        else if (depth == 0 && else_index != NULL
                 && *else_index < 0 && word_is(line, "else"))
        {
            *else_index = i;
        }
    }

    return -1;
}

/* ======================================================================
 *  4. Execution d'un bloc de lignes [from, to)
 * ====================================================================== */

/* Declaration anticipee : run_if / run_for / run_while appellent run_block
   pour executer leur corps (les blocs peuvent etre imbriques). */
static int run_block(Script *script, int from, int to);

/* Execute un bloc if ... then ... [else ...] fi commencant a la ligne i.
   Retourne l'index de la ligne apres le "fi" (ou -1 si erreur de syntaxe). */
static int run_if(Script *script, int i, int to)
{
    const char *line = skip_ws(script->lines[i]);
    int else_index;
    int fi = find_end(script, i, "fi", &else_index);

    if (fi < 0 || fi >= to)
    {
        syntax_error(i, "'if' sans 'fi' correspondant");
        return -1;
    }

    char condition[MAX_COMMAND_LENGTH];
    int inline_then = parse_header(line, 2, "then", condition,
                                   sizeof(condition));

    if (condition[0] == '\0')
    {
        syntax_error(i, "condition manquante apres 'if'");
        return -1;
    }

    /* Le bloc "then" s'arrete au "else" s'il existe, sinon au "fi" */
    int then_end = (else_index >= 0) ? else_index : fi;
    int start = body_start(script, i, inline_then, "then", then_end);

    if (start < 0)
    {
        syntax_error(i, "'then' attendu apres la condition du 'if'");
        return -1;
    }

    /* La condition est vraie si la commande retourne 0 (comme en bash). */
    if (execute_line(condition) == 0)
        run_block(script, start, then_end);
    else if (else_index >= 0)
        run_block(script, else_index + 1, fi);

    return fi + 1;
}

/* Execute for VAR in mot1 mot2 ... ; do ... done.
   Pour chaque mot : VAR prend sa valeur (setenv), puis le corps est execute.
   Retourne l'index de la ligne apres le "done". */
static int run_for(Script *script, int i, int to)
{
    const char *line = skip_ws(script->lines[i]);
    int done = find_end(script, i, "done", NULL);

    if (done < 0 || done >= to)
    {
        syntax_error(i, "'for' sans 'done' correspondant");
        return -1;
    }

    char header[MAX_COMMAND_LENGTH];
    int inline_do = parse_header(line, 3, "do", header, sizeof(header));
    int start = body_start(script, i, inline_do, "do", done);

    if (start < 0)
    {
        syntax_error(i, "'do' attendu apres l'en-tete du 'for'");
        return -1;
    }

    /* La liste de mots est developpee UNE fois, avant la boucle. */
    char arithmetic[MAX_COMMAND_LENGTH];
    char expanded[MAX_COMMAND_LENGTH];

    expand_arithmetic(header, arithmetic, sizeof(arithmetic));
    expand_variables(arithmetic, expanded, sizeof(expanded),
                     g_last_status, g_last_background_pid);

    char *variable = strtok(expanded, " \t");
    char *in_word = strtok(NULL, " \t");

    if (variable == NULL || in_word == NULL || strcmp(in_word, "in") != 0
        || !is_valid_name(variable))
    {
        syntax_error(i, "syntaxe attendue : for VAR in mot1 mot2 ...");
        return -1;
    }

    /* On decoupe tous les mots avant d'executer le corps, car execute_line
       utilise aussi strtok. */
    char *words[256];
    int word_count = 0;
    char *word;

    while ((word = strtok(NULL, " \t")) != NULL && word_count < 256)
        words[word_count++] = word;

    loop_depth++;

    for (int k = 0; k < word_count && g_running && !script_failed; k++)
    {
        /* La variable de boucle est une variable d'environnement : $VAR marche dans le corps */
        setenv(variable, words[k], 1);
        run_block(script, start, done);

        /* break : on sort de la boucle ; continue : on passe au mot suivant */
        if (loop_signal == 1)
        {
            loop_signal = 0;
            break;
        }

        if (loop_signal == 2)
            loop_signal = 0;
    }

    loop_depth--;
    return done + 1;
}

/* Execute while CONDITION ; do ... done : tant que la condition retourne 0,
   le corps est execute. Retourne l'index de la ligne apres le "done". */
static int run_while(Script *script, int i, int to)
{
    const char *line = skip_ws(script->lines[i]);
    int done = find_end(script, i, "done", NULL);

    if (done < 0 || done >= to)
    {
        syntax_error(i, "'while' sans 'done' correspondant");
        return -1;
    }

    char condition[MAX_COMMAND_LENGTH];
    int inline_do = parse_header(line, 5, "do", condition, sizeof(condition));

    if (condition[0] == '\0')
    {
        syntax_error(i, "condition manquante apres 'while'");
        return -1;
    }

    int start = body_start(script, i, inline_do, "do", done);

    if (start < 0)
    {
        syntax_error(i, "'do' attendu apres la condition du 'while'");
        return -1;
    }

    loop_depth++;

    /* La condition est reevaluee (avec ses variables) a chaque tour. */
    while (g_running && !script_failed && execute_line(condition) == 0)
    {
        run_block(script, start, done);

        /* break : on sort de la boucle ; continue : on reevalue la condition */
        if (loop_signal == 1)
        {
            loop_signal = 0;
            break;
        }

        if (loop_signal == 2)
            loop_signal = 0;
    }

    loop_depth--;
    return done + 1;
}

/* Execute les lignes [from, to) du script, une par une.
   Une ligne if / for / while est confiee a run_if / run_for / run_while, qui
   retournent l'index de la ligne suivant leur bloc ; une ligne simple est
   executee avec execute_line(). S'arrete sur quit, erreur, break ou continue. */
static int run_block(Script *script, int from, int to)
{
    int i = from;

    while (i < to && g_running && !script_failed && loop_signal == 0)
    {
        const char *line = skip_ws(script->lines[i]);

        /* lignes vides, commentaires et ligne "#!/..." */
        if (*line == '\0' || *line == '#')
        {
            i++;
            continue;
        }

        if (word_is(line, "if"))
        {
            i = run_if(script, i, to);
        }
        else if (word_is(line, "for"))
        {
            i = run_for(script, i, to);
        }
        else if (word_is(line, "while"))
        {
            i = run_while(script, i, to);
        }
        else if (word_is(line, "break") || word_is(line, "continue"))
        {
            if (loop_depth == 0)
            {
                syntax_error(i, "'break'/'continue' en dehors d'une boucle");
                return 2;
            }

            loop_signal = word_is(line, "break") ? 1 : 2;
            return g_last_status;
        }
        else if (word_is(line, "fi") || word_is(line, "done")
                 || word_is(line, "else") || word_is(line, "then")
                 || word_is(line, "do"))
        {
            syntax_error(i, "mot-cle inattendu");
            return 2;
        }
        else
        {
            execute_line(line);
            i++;
        }

        if (i < 0)   /* erreur de syntaxe dans un bloc */
            return 2;
    }

    return g_last_status;
}

/* Point d'entree du mode script (appele par main quand on lance
   ./ocsh fichier.ocsh). Retourne le code de retour du script :
   2 si erreur de syntaxe, sinon le code de la derniere commande. */
int run_script(const char *path)
{
    Script script;

    if (read_script(path, &script) == -1)
    {
        fprintf(stderr, "ocsh: %s: %s\n", path,
                errno != 0 ? strerror(errno) : "cannot read script");
        return 127;
    }

    script_name = path;

    run_block(&script, 0, script.count);

    free_script(&script);

    return script_failed ? 2 : g_last_status;
}
