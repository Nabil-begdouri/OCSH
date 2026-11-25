/*
 * pipes.c — Execution d'un pipeline : cmd1 -> cmd2 -> ... -> cmdN (jalons 1 et 3).
 *
 * Pour chaque commande on fait un fork(). Entre deux commandes voisines on cree
 * un tube avec pipe() : la sortie (stdout) de la commande i est branchee sur
 * l'entree (stdin) de la commande i+1 grace a dup2().
 *
 * Toutes les commandes d'un meme pipeline sont placees dans un meme groupe de
 * processus (setpgid) : c'est ce qui permet a Ctrl+C / Ctrl+Z d'agir sur tout le
 * pipeline et au job control (jobs, fg, bg) de le manipuler comme un seul job.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pipes.h"
#include "parser.h"
#include "shell.h"
#include "jobs.h"
#include "redirections.h"
#include "builtins.h"

/* Lance les command_count commandes de commands[].
   background = 1 si la ligne finit par '&' (on n'attend pas la fin).
   label      = texte de la commande, affiche par 'jobs'.
   last_pid   = recoit le PID de la derniere commande (pour $!).
   Retourne le code de retour de la derniere commande. */
int execute_pipeline(char *commands[], int command_count,
                     int background, const char *label, pid_t *last_pid)
{
    if (commands == NULL || command_count <= 0)
        return 1;

    if (command_count > MAX_COMMANDS)
    {
        fprintf(stderr, "ocsh: too many commands in pipeline\n");
        return 1;
    }

    /* Table des jobs pleine : impossible de lancer un job de plus en arriere-plan */
    if (background && !jobs_has_free_slot())
    {
        fprintf(stderr, "ocsh: too many background jobs\n");
        return 1;
    }

    /* On BLOQUE SIGCHLD pendant la creation des fils : sinon le gestionnaire
       SIGCHLD pourrait recolter un fils (waitpid) avant qu'il soit enregistre,
       et on perdrait son code de retour (course critique). */
    sigset_t block_mask;
    sigset_t old_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1)
    {
        perror("ocsh: sigprocmask");
        return 1;
    }

    /* previous_fd : extremite LECTURE du tube precedent (stdin de la commande
       courante), -1 pour la premiere commande.
       pgid : identifiant du groupe de processus = PID du premier fils. */
    int previous_fd = -1;
    int pipe_fd[2];
    pid_t pids[MAX_COMMANDS];
    int started = 0;
    pid_t pgid = 0;

    for (int i = 0; i < command_count; i++)
    {
        char *args[MAX_ARGS];

        /* Decoupe la commande i en arguments (gere les guillemets) */
        tokenize(commands[i], args, MAX_ARGS);

        if (args[0] == NULL)
        {
            fprintf(stderr, "ocsh: empty command in pipeline\n");
            goto error;
        }

        /* Si ce n'est pas la derniere commande, on cree le tube vers la suivante.
           pipe_fd[0] = lecture, pipe_fd[1] = ecriture. */
        if (i < command_count - 1)
        {
            if (pipe(pipe_fd) == -1)
            {
                perror("ocsh: pipe");
                goto error;
            }
        }

        /* Un processus fils par commande du pipeline */
        pid_t pid = fork();

        if (pid == -1)
        {
            perror("ocsh: fork");

            if (i < command_count - 1)
            {
                close(pipe_fd[0]);
                close(pipe_fd[1]);
            }

            goto error;
        }

        if (pid == 0)
        {
            /* ===== PROCESSUS FILS =====
               Le fils retrouve le masque de signaux normal. */
            sigprocmask(SIG_SETMASK, &old_mask, NULL);

            /* Le fils rejoint le groupe du pipeline (ou en cree un si pgid == 0) */
            setpgid(0, pgid);

            /* Commande au premier plan : son groupe recoit le terminal (tcsetpgrp),
               c'est donc lui qui recevra Ctrl+C et Ctrl+Z, et non le shell. */
            if (!background && jobs_interactive())
                tcsetpgrp(STDIN_FILENO, getpgrp());

            /* Le shell ignore ces signaux ; le fils remet le comportement par defaut
               pour que Ctrl+C tue la commande (et pas le shell). */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            /* Commande en arriere-plan : on lui donne /dev/null comme entree,
               pour qu'elle ne lise pas le clavier en meme temps que le shell. */
            if (background && previous_fd == -1)
            {
                int null_fd = open("/dev/null", O_RDONLY);

                if (null_fd != -1)
                {
                    dup2(null_fd, STDIN_FILENO);
                    close(null_fd);
                }
            }

            /* Pas la premiere commande : stdin <- extremite lecture du tube precedent */
            if (previous_fd != -1)
            {
                if (dup2(previous_fd, STDIN_FILENO) == -1)
                {
                    perror("ocsh: dup2");
                    _exit(126);
                }

                close(previous_fd);
            }

            if (i < command_count - 1)
            {
                /* Pas la derniere commande : stdout -> extremite ecriture du tube.
                   On ferme la lecture, inutile dans ce fils. */
                close(pipe_fd[0]);

                if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
                {
                    perror("ocsh: dup2");
                    _exit(126);
                }

                /* Apres dup2, le descripteur d'origine est inutile : on le ferme
                   (un tube reste ouvert tant qu'une extremite d'ecriture existe : sans
                   ces close(), la commande suivante n'aurait jamais EOF). */
                close(pipe_fd[1]);
            }

            /* Redirections */
            if (handle_redirection(args) == -1)
                _exit(1);

            /* Builtin dans un pipeline ou en arriere-plan (say hi -> wc -c,
               osinfo &) : il s'execute dans ce fils, puis le fils se termine. */
            int builtin_status = execute_builtin_in_child(args);

            if (builtin_status != -1)
            {
                fflush(stdout);
                _exit(builtin_status);
            }

            /* Commande externe : execvp remplace l'image memoire du fils par le
               programme (cherche dans PATH). En cas de succes, il ne revient jamais. */
            execvp(args[0], args);

            /* execvp a echoue : commande introuvable -> code 127 (comme Bash) */
            fprintf(stderr, "ocsh: %s: %s\n",
                    args[0], strerror(errno));
            _exit(127);
        }

        /* ===== PROCESSUS PERE (le shell) =====
           Le premier fils devient le chef du groupe. On appelle aussi setpgid
           dans le pere pour eviter une course avec le fils. */
        if (pgid == 0)
            pgid = pid;

        setpgid(pid, pgid);

        /* On memorise le PID pour attendre / gerer le job ensuite */
        pids[started++] = pid;

        if (previous_fd != -1)
            /* Le pere ferme les extremites des tubes qu'il n'utilise pas :
               il garde seulement la lecture du dernier tube pour la commande suivante. */
            close(previous_fd);

        if (i < command_count - 1)
        {
            close(pipe_fd[1]);
            previous_fd = pipe_fd[0];
        }
    }

    /* Arriere-plan : on enregistre le job, on affiche [n] PID et on rend
       la main tout de suite, sans attendre (le gestionnaire SIGCHLD recoltera
       les fils a leur fin, pour qu'ils ne restent pas zombies). */
    if (background)
    {
        int job_id = jobs_add(pids, started, label, pgid, 0);

        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        if (last_pid != NULL)
            *last_pid = pids[started - 1];

        printf("[%d] %d\n", job_id, (int)pids[started - 1]);
        return 0;
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    /* Premier plan : on attend la fin (ou l'arret par Ctrl+Z) de tout le
       pipeline. Retourne le code de retour de la derniere commande. */
    return jobs_wait_foreground(pgid, pids, started, label);

/* En cas d'erreur au milieu : on ferme le tube en cours, on tue les fils
   deja lances et on les attend (pas de zombie), puis on retablit le masque. */
error:

    if (previous_fd != -1)
        close(previous_fd);

    for (int i = 0; i < started; i++)
    {
        kill(pids[i], SIGTERM);

        while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR)
        {
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return 1;
}