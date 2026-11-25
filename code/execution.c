/*
 * execution.c — Execution d'une commande externe simple (jalon 1).
 *
 * C'est le schema de base de tout shell :
 *     fork()  -> le shell se duplique (pere + fils)
 *     execvp()-> dans le fils, on remplace l'image memoire par le programme
 *     waitpid()-> le pere attend la fin du fils et recupere son code de retour
 *
 * Remarque : c'est la version du jalon 1, gardee comme reference.
 * Le shell final utilise execute_pipeline() (pipes.c), qui reprend exactement
 * ce schema en le generalisant a N commandes (pipes, groupes, job control).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "execution.h"

/* Lance args[0] avec ses arguments et attend sa fin.
   Retourne le code de retour de la commande (comme $? dans Bash). */
int execute_command(char **args)
{
    /* Ligne vide : rien a executer */
    if (args == NULL || args[0] == NULL)
        return 0;

    /* fork() cree un processus fils, copie du shell.
       Il retourne 0 dans le fils, le PID du fils dans le pere, -1 en cas d'erreur. */
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("ocsh: fork");
        return 1;
    }

    if (pid == 0)
    {
        /* --- Processus FILS --- */

        /* Le shell ignore Ctrl+C ; le fils doit le reprendre par defaut
           pour que Ctrl+C tue la commande et pas le shell. */
        signal(SIGINT, SIG_DFL);

        /* execvp cherche le programme dans PATH et remplace l'image memoire
           du fils (le PID reste le meme). S'il reussit, il ne revient JAMAIS. */
        execvp(args[0], args);

        /* Si on arrive ici, execvp a echoue (commande introuvable...). */
        fprintf(stderr, "ocsh: %s: %s\n", args[0], strerror(errno));

        /* _exit (et non exit) : on quitte sans vider les tampons stdio
           herites du pere. 127 = "commande introuvable", comme Bash. */
        _exit(127);
    }

    /* --- Processus PERE (le shell) --- */
    int status;
    pid_t result;

    /* On attend le fils. waitpid peut etre interrompu par un signal (EINTR) :
       dans ce cas on recommence. Attendre le fils evite qu'il reste zombie. */
    do
    {
        result = waitpid(pid, &status, 0);
    }
    while (result == -1 && errno == EINTR);

    if (result == -1)
    {
        perror("ocsh: waitpid");
        return 1;
    }

    /* Le fils s'est termine normalement : on renvoie son code (exit) */
    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    /* Le fils a ete tue par un signal (ex : Ctrl+C) :
       convention Bash, code = 128 + numero du signal. */
    if (WIFSIGNALED(status))
    {
        if (WTERMSIG(status) == SIGINT)
            printf("\n");

        return 128 + WTERMSIG(status);
    }

    return 1;
}
