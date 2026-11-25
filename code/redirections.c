/*
 * redirections.c — Redirections d'entree/sortie (jalon 2).
 *
 *   cmd > fichier    stdout ecrase le fichier
 *   cmd >> fichier   stdout ajoute a la fin du fichier
 *   cmd < fichier    stdin lit le fichier
 * On ouvre le fichier avec open(), puis dup2() le met a la place du
 * descripteur 0 (stdin) ou 1 (stdout) : le programme ne sait meme pas
 * qu'il ecrit dans un fichier.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "redirections.h"

/* Ouvre 'file' et le branche sur 'target_fd' (0 = stdin, 1 = stdout)
   grace a dup2. Retourne 0 si OK, -1 en cas d'erreur. */
static int redirect_to(const char *file, int flags, int target_fd)
{
    int fd = open(file, flags, 0644);

    if (fd == -1)
    {
        perror(file);
        return -1;
    }

    if (dup2(fd, target_fd) == -1)
    {
        perror("ocsh: dup2");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* Traite TOUTES les redirections de la commande (ex: sort < in > out).
   Les operateurs et noms de fichiers sont retires de args (compactage),
   pour que execvp ne recoive que la commande et ses arguments.
   Retourne le nombre de redirections faites, ou -1 en cas d'erreur. */
int handle_redirection(char **args)
{
    int read_index = 0;
    int write_index = 0;
    int count = 0;

    while (args[read_index] != NULL)
    {
        const char *op = args[read_index];
        int flags = -1;
        int target = -1;

        if (strcmp(op, ">>") == 0)
        {
            flags = O_WRONLY | O_CREAT | O_APPEND;
            target = STDOUT_FILENO;
        }
        else if (strcmp(op, ">") == 0)
        {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            target = STDOUT_FILENO;
        }
        else if (strcmp(op, "<") == 0)
        {
            flags = O_RDONLY;
            target = STDIN_FILENO;
        }

        if (target == -1)
        {
            /* argument normal : on le garde */
            args[write_index++] = args[read_index++];
            continue;
        }

        if (args[read_index + 1] == NULL)
        {
            fprintf(stderr, "ocsh: fichier manquant apres '%s'\n", op);
            args[write_index] = NULL;
            return -1;
        }

        if (redirect_to(args[read_index + 1], flags, target) == -1)
        {
            args[write_index] = NULL;
            return -1;
        }

        count++;
        read_index += 2;   /* on saute l'operateur et le nom de fichier */
    }

    args[write_index] = NULL;
    return count;
}
