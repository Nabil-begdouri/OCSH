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
#include "parser.h"   /* MAX_COMMANDS */
#include "shell.h"    /* MAX_ARGS */
#include "jobs.h"

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

    if (background && !jobs_has_free_slot())
    {
        fprintf(stderr, "ocsh: too many background jobs\n");
        return 1;
    }

    /* On bloque SIGCHLD pendant les fork() : si un fils se termine tres vite,
       le signal reste en attente jusqu'a ce que le job soit enregistre. */
    sigset_t block_mask;
    sigset_t old_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1)
    {
        perror("ocsh: sigprocmask");
        return 1;
    }

    int previous_fd = -1;
    int pipe_fd[2];
    pid_t pids[MAX_COMMANDS];
    int started = 0;
    pid_t pgid = 0;   /* groupe de processus du job */

    for (int i = 0; i < command_count; i++)
    {
        char *args[MAX_ARGS];
        int j = 0;

        char *token = strtok(commands[i], " \t");

        while (token != NULL && j < MAX_ARGS - 1)
        {
            args[j++] = token;
            token = strtok(NULL, " \t");
        }

        args[j] = NULL;

        if (args[0] == NULL)
        {
            fprintf(stderr, "ocsh: empty command in pipeline\n");
            goto error;
        }

        if (i < command_count - 1)
        {
            if (pipe(pipe_fd) == -1)
            {
                perror("ocsh: pipe");
                goto error;
            }
        }

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
            /* --- processus fils --- */

            /* Le masque de signaux est herite par exec : on le remet. */
            sigprocmask(SIG_SETMASK, &old_mask, NULL);

            /* Chaque job a son propre groupe de processus : c'est ce qui
               permet d'envoyer Ctrl+C / Ctrl+Z au job seul, pas au shell.
               pgid == 0 : le premier processus cree le groupe. */
            setpgid(0, pgid);

            /* Premier plan : le groupe prend le clavier (a faire AVANT de
               remettre SIGTTOU par defaut, sinon le fils serait suspendu). */
            if (!background && jobs_interactive())
                tcsetpgrp(STDIN_FILENO, getpgrp());

            /* Le shell ignore ces signaux et "ignore" survit a exec :
               le fils doit retrouver le comportement par defaut. */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            if (background && previous_fd == -1)
            {
                /* Un job d'arriere-plan ne doit pas lire le clavier. */
                int null_fd = open("/dev/null", O_RDONLY);

                if (null_fd != -1)
                {
                    dup2(null_fd, STDIN_FILENO);
                    close(null_fd);
                }
            }

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
                close(pipe_fd[0]);

                if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
                {
                    perror("ocsh: dup2");
                    _exit(126);
                }

                close(pipe_fd[1]);
            }

            execvp(args[0], args);

            fprintf(stderr, "ocsh: %s: %s\n", args[0], strerror(errno));
            _exit(127);
        }

        /* --- processus pere --- */

        if (pgid == 0)
            pgid = pid;

        /* Fait aussi par le fils : evite une course entre les deux. */
        setpgid(pid, pgid);

        pids[started++] = pid;

        if (previous_fd != -1)
            close(previous_fd);

        if (i < command_count - 1)
        {
            close(pipe_fd[1]);
            previous_fd = pipe_fd[0];
        }
    }

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

    /* Premier plan : on donne le terminal au groupe et on attend
       (gere aussi Ctrl+Z : le job est alors enregistre comme "Stopped"). */
    return jobs_wait_foreground(pgid, pids, started, label);

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
