#ifndef HISTORY_H
#define HISTORY_H

#define MAX_COMMAND_LENGTH 1024
#define HISTORY_SIZE 100

void add_custom_history(const char *command);
void display_custom_history(void);

#endif
