#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include "execution.h"
#include "builtins.h"
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
int execute_command(char **args)
{
    if (args == NULL || args[0] == NULL)
        return 0;
   if (strcmp(args[0], "go") == 0)
    return execute_builtin(args);
     
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("ocsh: fork");
        return 1;
    }

  if (pid == 0)
{
    int redirection = handle_redirection(args);

    if (redirection == -1)
        _exit(1);

    int builtin_status = execute_builtin(args);

    if (builtin_status != -1)
        _exit(builtin_status);

    execvp(args[0], args);
    perror("ocsh");
    _exit(127);
}

    int status;

    if (waitpid(pid, &status, 0) == -1)
    {
        perror("ocsh: waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}

