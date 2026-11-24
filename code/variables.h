#ifndef VARIABLES_H
#define VARIABLES_H

#include <stddef.h>

void expand_variables(char *input, char *output, size_t size, int last_status);
int handle_export(char *argument);

#endif
