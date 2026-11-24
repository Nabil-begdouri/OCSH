#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "redirections.h"

int handle_redirection(char **args)
{
    for (int i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], ">>") == 0)
        {
            if (args[i + 1] == NULL)
            {
                fprintf(stderr, "Erreur : fichier manquant\n");
                return -1;
            }

            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);

            if (fd == -1)
            {
                perror("open");
                return -1;
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
            return 1;
        }

        if (strcmp(args[i], ">") == 0)
        {
            if (args[i + 1] == NULL)
            {
                fprintf(stderr, "Erreur : fichier manquant\n");
                return -1;
            }

            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd == -1)
            {
                perror("open");
                return -1;
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
            return 1;
        }

        if (strcmp(args[i], "<") == 0)
        {
            if (args[i + 1] == NULL)
            {
                fprintf(stderr, "Erreur : fichier manquant\n");
                return -1;
            }

            int fd = open(args[i + 1], O_RDONLY);

            if (fd == -1)
            {
                perror("open");
                return -1;
            }

            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
            return 1;
        }
    }

    return 0;
}