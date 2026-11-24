#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "jobs.h"
#include "parser.h"   /* MAX_COMMANDS */

#define MAX_JOBS 64
#define MAX_JOB_COMMAND 256

/* Un job = une ligne (commande ou pipeline) suivie par le shell. */
typedef struct
{
    volatile sig_atomic_t active;     /* case utilisee ?                  */
    volatile sig_atomic_t finished;   /* tous les processus sont termines */
    volatile sig_atomic_t remaining;  /* processus encore vivants         */
    volatile sig_atomic_t stopped;    /* suspendu (Ctrl+Z / SIGSTOP)      */
    pid_t pgid;                       /* groupe de processus du job       */
    pid_t pids[MAX_COMMANDS];         /* 0 = deja recolte                 */
    int count;
    char command[MAX_JOB_COMMAND];
} Job;

static Job jobs[MAX_JOBS];

static int interactive = 0;     /* stdin est un terminal ?     */
static pid_t shell_pgid = 0;    /* groupe de processus du shell */

/* ----------------------------------------------------------------------
 *  SIGCHLD : on recolte les fils d'arriere-plan termines (pas de zombies).
 *  Seulement des appels "async-signal-safe" : pas de printf ici.
 *  Les jobs suspendus sont ignores (waitpid sans WUNTRACED ne les voit pas).
 * -------------------------------------------------------------------- */
static void handle_sigchld(int signal_number)
{
    (void)signal_number;

    int saved_errno = errno;
    int status;

    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (!jobs[j].active || jobs[j].finished || jobs[j].stopped)
            continue;

        for (int k = 0; k < jobs[j].count; k++)
        {
            pid_t pid = jobs[j].pids[k];

            if (pid <= 0)
                continue;

            pid_t result = waitpid(pid, &status, WNOHANG);

            if (result == pid || (result == -1 && errno == ECHILD))
            {
                jobs[j].pids[k] = 0;
                jobs[j].remaining--;
            }
        }

        if (jobs[j].remaining <= 0)
            jobs[j].finished = 1;
    }

    errno = saved_errno;
}

void setup_jobs(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigchld;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &action, NULL) == -1)
        perror("ocsh: sigaction");

    interactive = isatty(STDIN_FILENO);

    if (interactive)
    {
        /* Le shell ne doit pas etre suspendu par Ctrl+Z, ni par les
           signaux du terminal quand il manipule le groupe de premier plan. */
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        /* Le shell devient chef de son propre groupe et prend le terminal. */
        setpgid(0, 0);   /* peut echouer (deja chef de session) : sans gravite */
        shell_pgid = getpgrp();
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }
    else
    {
        shell_pgid = getpgrp();
    }
}

int jobs_interactive(void)
{
    return interactive;
}

static void block_sigchld(sigset_t *old_mask)
{
    sigset_t block_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, old_mask);
}

int jobs_has_free_slot(void)
{
    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (!jobs[j].active)
            return 1;
    }

    return 0;
}

int jobs_add(const pid_t *pids, int count, const char *command,
             pid_t pgid, int stopped)
{
    if (pids == NULL || count <= 0 || count > MAX_COMMANDS)
        return -1;

    sigset_t old_mask;
    block_sigchld(&old_mask);

    int slot = -1;

    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (!jobs[j].active)
        {
            slot = j;
            break;
        }
    }

    if (slot != -1)
    {
        Job *job = &jobs[slot];
        int alive = 0;

        for (int k = 0; k < count; k++)
        {
            job->pids[k] = pids[k];

            if (pids[k] > 0)
                alive++;
        }

        job->count = count;
        job->remaining = alive;
        job->finished = 0;
        job->stopped = stopped;
        job->pgid = pgid;

        strncpy(job->command, command != NULL ? command : "",
                MAX_JOB_COMMAND - 1);
        job->command[MAX_JOB_COMMAND - 1] = '\0';

        job->active = 1;   /* en dernier : la case devient visible */
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return slot == -1 ? -1 : slot + 1;
}

/* ----------------------------------------------------------------------
 *  Attente d'un groupe de processus
 * -------------------------------------------------------------------- */

/* Attend les processus de 'pids' (mis a 0 quand ils sont termines).
   *stopped = 1 si l'un d'eux est suspendu. Retourne le code de retour de
   la derniere commande (148 si le job a ete suspendu par Ctrl+Z). */
static int wait_pids(pid_t *pids, int count, int *stopped)
{
    int last_status = 0;
    int stop_status = 0;

    *stopped = 0;

    for (int i = 0; i < count; i++)
    {
        if (pids[i] <= 0)
            continue;

        int status;
        pid_t result;

        do
        {
            result = waitpid(pids[i], &status, WUNTRACED);
        }
        while (result == -1 && errno == EINTR);

        if (result == -1)
        {
            if (errno != ECHILD)
                perror("ocsh: waitpid");

            pids[i] = 0;
            last_status = 1;
            continue;
        }

        if (WIFSTOPPED(status))
        {
            *stopped = 1;
            stop_status = 128 + WSTOPSIG(status);
            continue;   /* le processus reste dans le job */
        }

        pids[i] = 0;

        if (i == count - 1)
        {
            if (WIFEXITED(status))
            {
                last_status = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status))
            {
                last_status = 128 + WTERMSIG(status);

                if (WTERMSIG(status) == SIGINT)
                    printf("\n");   /* Ctrl+C : on revient a la ligne */
            }
        }
    }

    return *stopped ? stop_status : last_status;
}

static void give_terminal(pid_t pgid)
{
    if (interactive)
        tcsetpgrp(STDIN_FILENO, pgid);
}

int jobs_wait_foreground(pid_t pgid, pid_t *pids, int count,
                         const char *label)
{
    int stopped;

    give_terminal(pgid);

    int status = wait_pids(pids, count, &stopped);

    give_terminal(shell_pgid);   /* le shell reprend le clavier */

    if (stopped)
    {
        int id = jobs_add(pids, count, label, pgid, 1);

        printf("\n[%d]+  %-22s %s\n", id, "Stopped", label);
        fflush(stdout);
    }

    return status;
}

/* ----------------------------------------------------------------------
 *  Notifications et builtins
 * -------------------------------------------------------------------- */

void jobs_notify_finished(void)
{
    sigset_t old_mask;
    block_sigchld(&old_mask);

    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (jobs[j].active && jobs[j].finished)
        {
            printf("[%d]+  Done                    %s\n",
                   j + 1, jobs[j].command);
            jobs[j].active = 0;
        }
    }

    fflush(stdout);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

void jobs_print(void)
{
    sigset_t old_mask;
    block_sigchld(&old_mask);

    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (!jobs[j].active)
            continue;

        const char *state = jobs[j].finished ? "Done"
                          : jobs[j].stopped  ? "Stopped"
                                             : "Running";

        printf("[%d]+  %-22s %s\n", j + 1, state, jobs[j].command);

        if (jobs[j].finished)
            jobs[j].active = 0;
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

int jobs_parse_id(const char *text)
{
    if (text == NULL)
        return 0;

    if (*text == '%')
        text++;

    return atoi(text);
}

/* Cherche un job vivant : celui de numero 'id', ou (id = 0) le plus recent.
   only_stopped = 1 : on ne retient que les jobs suspendus (pour bg). */
static int find_job(int id, int only_stopped)
{
    if (id > 0)
    {
        int j = id - 1;

        if (j < MAX_JOBS && jobs[j].active && !jobs[j].finished
            && (!only_stopped || jobs[j].stopped))
            return j;

        return -1;
    }

    for (int j = MAX_JOBS - 1; j >= 0; j--)
    {
        if (jobs[j].active && !jobs[j].finished
            && (!only_stopped || jobs[j].stopped))
            return j;
    }

    return -1;
}

int jobs_fg(int id)
{
    /* SIGCHLD bloque : sinon le gestionnaire recolterait les fils
       avant notre waitpid et on perdrait leur code de retour. */
    sigset_t old_mask;
    block_sigchld(&old_mask);

    int j = find_job(id, 0);

    if (j < 0)
    {
        fprintf(stderr, "ocsh: fg: no such job\n");
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return 1;
    }

    Job *job = &jobs[j];

    printf("%s\n", job->command);
    fflush(stdout);

    give_terminal(job->pgid);

    if (job->stopped)
    {
        job->stopped = 0;
        kill(-job->pgid, SIGCONT);   /* reveille tout le groupe */
    }

    int stopped;
    int status = wait_pids(job->pids, job->count, &stopped);

    give_terminal(shell_pgid);

    if (stopped)
    {
        job->stopped = 1;
        printf("\n[%d]+  %-22s %s\n", j + 1, "Stopped", job->command);
        fflush(stdout);
    }
    else
    {
        job->active = 0;
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return status;
}

int jobs_bg(int id)
{
    sigset_t old_mask;
    block_sigchld(&old_mask);

    int j = find_job(id, 1);

    if (j < 0)
    {
        fprintf(stderr, "ocsh: bg: no stopped job\n");
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        return 1;
    }

    Job *job = &jobs[j];

    job->stopped = 0;
    kill(-job->pgid, SIGCONT);

    printf("[%d]+ %s &\n", j + 1, job->command);
    fflush(stdout);

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return 0;
}

void jobs_cleanup(void)
{
    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (jobs[j].active && jobs[j].stopped && jobs[j].pgid > 0)
        {
            kill(-jobs[j].pgid, SIGHUP);
            kill(-jobs[j].pgid, SIGCONT);
        }
    }
}
