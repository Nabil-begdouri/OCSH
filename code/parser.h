#ifndef PARSER_H
#define PARSER_H

#define MAX_COMMANDS 16

int split_pipeline(char *input, char *commands[], int max_commands);
int tokenize(char *line, char *args[], int max_args);

#endif