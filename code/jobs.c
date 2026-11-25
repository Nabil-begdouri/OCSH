/*
 * jobs.c — Arriere-plan, signaux et job control (jalons 3 et 6).
 *
 *   - commande &          : le job tourne en arriere-plan, le shell n'attend pas
 *   - SIGCHLD             : le noyau previent le shell qu'un fils est termine ;
 *                           le gestionnaire le recolte (waitpid) -> pas de zombie
 *   - Ctrl+C / Ctrl+Z     : envoyes au groupe de processus au premier plan
 *   - jobs / fg / bg      : lister, ramener au premier plan, relancer en fond
 *
 * Le shell garde une table de 64 jobs. Un job = une ligne de commande
 * (une commande seule ou tout un pipeline), avec son groupe de processus.
 */

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

/* Taille maximale de la table des jobs et du texte memorise par job */
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

/* Table des jobs, partagee avec le gestionnaire SIGCHLD.
   Les champs modifies par le gestionnaire sont 'volatile sig_atomic_t'
   (lecture/ecriture sures meme si un signal arrive au milieu). */
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

    /* Le gestionnaire peut interrompre n'importe quel code : on sauvegarde
       errno pour ne pas fausser une erreur en cours dans le programme. */
    int saved_errno = errno;
    int status;

    for (int j = 0; j < MAX_JOBS; j++)
    {
        /* On ne s'occupe que des jobs vivants et non suspendus */
        if (!jobs[j].active || jobs[j].finished || jobs[j].stopped)
            continue;

        for (int k = 0; k < jobs[j].count; k++)
        {
            pid_t pid = jobs[j].pids[k];

            if (pid <= 0)
                continue;

            /* WNOHANG : ne bloque pas. Retourne pid si ce fils est termine (il est
               alors recolte : il ne reste pas zombie), 0 s'il tourne encore. */
            pid_t result = waitpid(pid, &status, WNOHANG);

            if (result == pid || (result == -1 && errno == ECHILD))
            {
                jobs[j].pids[k] = 0;
                jobs[j].remaining--;
            }
        }

        /* Plus aucun processus vivant : le job est termine (affiche "Done" plus tard) */
        if (jobs[j].remaining <= 0)
            jobs[j].finished = 1;
    }

    errno = saved_errno;
}

/* Initialisation, appelee une fois au demarrage du shell (main).
   Installe le gestionnaire SIGCHLD et, en mode interactif, prepare le
   shell pour le job control (groupe de processus et terminal). */
void setup_jobs(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigchld;
    sigemptyset(&action.sa_mask);
    /* SA_RESTART  : un appel systeme interrompu (ex : readline) reprend tout seul
       SA_NOCLDSTOP : pas de SIGCHLD quand un fils est seulement suspendu (Ctrl+Z) */
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &action, NULL) == -1)
        perror("ocsh: sigaction");

    /* Mode interactif = l'entree est un vrai terminal (pas un script ni un pipe) */
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
        /* Mode non interactif : pas de gestion du terminal */
        shell_pgid = getpgrp();
    }
}

/* Indique si le job control (terminal) est actif */
int jobs_interactive(void)
{
    return interactive;
}

/* Desactive la gestion du terminal (utilise dans le fils de $(...)) */
void jobs_disable_interactive(void)
{
    interactive = 0;
}

/* Bloque SIGCHLD (le signal est mis en attente, pas perdu) et sauvegarde
   l'ancien masque dans old_mask pour pouvoir le retablir avec sigprocmask.
   Sert a eviter que le gestionnaire modifie la table pendant qu'on la lit. */
static void block_sigchld(sigset_t *old_mask)
{
    sigset_t block_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, old_mask);
}

/* Retourne 1 s'il reste une case libre dans la table des jobs */
int jobs_has_free_slot(void)
{
    for (int j = 0; j < MAX_JOBS; j++)
    {
        if (!jobs[j].active)
            return 1;
    }

    return 0;
}

/* Enregistre un nouveau job : ses PID, son texte et son groupe (pgid).
   stopped = 1 si le job est deja suspendu (Ctrl+Z).
   Retourne le numero du job (1, 2, ...) affiche par [n], ou -1 si plein. */
int jobs_add(const pid_t *pids, int count, const char *command,
             pid_t pgid, int stopped)
{
    if (pids == NULL || count <= 0 || count > MAX_COMMANDS)
        return -1;

    sigset_t old_mask;
    block_sigchld(&old_mask);

    /* Recherche de la premiere case libre */
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

        /* remaining = nombre de processus encore vivants dans le job */
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
            /* WUNTRACED : waitpid revient aussi quand le fils est SUSPENDU (Ctrl+Z),
               pas seulement quand il se termine */
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

        /* Le fils a ete suspendu par Ctrl+Z : code 128 + numero du signal */
        if (WIFSTOPPED(status))
        {
            *stopped = 1;
            stop_status = 128 + WSTOPSIG(status);
            continue;   /* le processus reste dans le job */
        }

        pids[i] = 0;

        /* Le code de retour d'un pipeline est celui de la DERNIERE commande ($?) */
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

/* Donne le terminal au groupe pgid (tcsetpgrp) : c'est ce groupe qui
   recevra le clavier et les signaux Ctrl+C / Ctrl+Z. */
static void give_terminal(pid_t pgid)
{
    if (interactive)
        tcsetpgrp(STDIN_FILENO, pgid);
}

/* Attend un job lance au premier plan.
   1) le terminal est donne au groupe du job (il recoit Ctrl+C / Ctrl+Z)
   2) on attend ses processus
   3) le shell reprend le terminal
   Si le job a ete suspendu, il est ajoute a la table (pour fg / bg). */
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

/* Appelee avant chaque prompt : affiche "[n]+ Done" pour les jobs
   d'arriere-plan termines, puis libere leur case. */
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

/* Builtin jobs : liste les jobs avec leur etat (Running, Stopped, Done) */
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

/* Convertit l'argument de fg / bg en numero de job : "2" ou "%2" -> 2 */
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

/* Builtin fg [n] : ramene le job n (ou le plus recent) au premier plan.
   On lui donne le terminal, on le reveille avec SIGCONT s'il etait
   suspendu, puis on l'attend comme une commande normale. */
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
        /* kill avec un PID negatif : le signal est envoye a TOUT le groupe
           (toutes les commandes du pipeline) */
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

/* Builtin bg [n] : relance en arriere-plan un job suspendu par Ctrl+Z.
   Il est reveille avec SIGCONT mais le shell ne l'attend pas. */
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

/* Appelee quand on quitte le shell (quit) : les jobs encore suspendus
   recoivent SIGHUP (fin de session) puis SIGCONT pour pouvoir le traiter
   et se terminer, au lieu de rester bloques pour toujours. */
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
