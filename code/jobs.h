#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

void setup_jobs(void);
int execute_background(char *args[], pid_t *last_background_pid);

#endif