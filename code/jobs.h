/* jobs.h — Arriere-plan, signaux (SIGCHLD...) et job control (jalons 3 et 6). Code : jobs.c */

#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

/* Installe SIGCHLD et, si stdin est un terminal, prepare le controle de jobs
   (le shell devient chef de son groupe, prend le terminal, ignore Ctrl+Z). */
void setup_jobs(void);

/* 1 si le shell est interactif (stdin = terminal). */
int jobs_interactive(void);

/* Desactive le controle de terminal (utilise dans le fils de $(...)). */
void jobs_disable_interactive(void);

/* 1 s'il reste de la place dans la table des jobs. */
int jobs_has_free_slot(void);

/* Enregistre un job (un pipeline = plusieurs PID, groupe 'pgid').
   stopped = 1 si le job est suspendu (Ctrl+Z).
   Retourne le numero du job (1, 2, ...) ou -1 si la table est pleine. */
int jobs_add(const pid_t *pids, int count, const char *command,
             pid_t pgid, int stopped);

/* Attend un job lance au premier plan. Donne le terminal au groupe 'pgid',
   attend avec WUNTRACED, puis reprend le terminal. Si le job est suspendu
   (Ctrl+Z) il est enregistre comme "Stopped". Retourne le code de retour. */
int jobs_wait_foreground(pid_t pgid, pid_t *pids, int count,
                         const char *label);

/* Affiche "[n]+ Done ..." pour les jobs termines (avant chaque prompt). */
void jobs_notify_finished(void);

/* Builtins */
void jobs_print(void);
int  jobs_fg(int id);   /* id = 0 : job courant (le plus recent) */
int  jobs_bg(int id);
int  jobs_parse_id(const char *text);   /* "%2" ou "2" -> 2 ; NULL -> 0 */

/* A la sortie du shell : reveille et termine les jobs suspendus. */
void jobs_cleanup(void);

#endif
