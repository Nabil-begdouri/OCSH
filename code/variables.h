#ifndef VARIABLES_H
#define VARIABLES_H

#include <stddef.h>
#include <sys/types.h>

void expand_variables(char *input, char *output, size_t size,
                      int last_status, pid_t last_background_pid);

int execute_export(char *args[]);

#endif