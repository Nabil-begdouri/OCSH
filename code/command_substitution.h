#ifndef COMMAND_SUBSTITUTION_H
#define COMMAND_SUBSTITUTION_H

#include <stddef.h>

void expand_command_substitution(char *input, char *output, size_t size);

#endif
