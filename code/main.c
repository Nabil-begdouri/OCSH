#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>
#include "execution.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define HISTORY_SIZE 100

char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
int history_count = 0;

char *command_generator(const char *text, int state)
{
    static int index;
    static char *commands[] = {"go", "path", "info", "say", "osinfo", "quit", "past", "export", "list", "show", "copy", "move", "del", "newdir", "newfile", NULL};
    if (!state) index = 0;
    while (commands[index])
    {
        char *command = commands[index++];
        if (strncmp(command, text, strlen(text)) == 0)
            return strdup(command);
    }
    return NULL;
}

char **command_completion(const char *text, int start, int end)
{
    (void)start;
    (void)end;
    return rl_completion_matches(text, command_generator);
}



void expand_command_substitution(char *input, char *output, size_t size)
{
    size_t i = 0, j = 0;

    while (input[i] != '\0' && j < size - 1)
    {
        if ((input[i] == '$' && input[i + 1] == '(') || input[i] == '`')
        {
            int backtick = (input[i] == '`');
            i += backtick ? 1 : 2;
            int depth = backtick ? 0 : 1;
            char inner[512];
            size_t k = 0;

            while (input[i] != '\0' && (backtick || depth > 0))
            {
                if (backtick)
                {
                    if (input[i] == '`')
                    {
                        i++;
                        break;
                    }
                }
                else
                {
                    if (input[i] == '(')
                        depth++;
                    else if (input[i] == ')')
                    {
                        depth--;
                        if (depth == 0)
                        {
                            i++;
                            break;
                        }
                    }
                }

                if ((backtick || depth > 0) && k < sizeof(inner) - 1)
                    inner[k++] = input[i];

                i++;
            }

            inner[k] = '\0';

            int pipefd[2];
            if (pipe(pipefd) == -1)
                continue;

            pid_t pid = fork();

            if (pid == 0)
            {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                char *args[MAX_ARGS];
                int argc = 0;
                char *token = strtok(inner, " \t");

                while (token != NULL && argc < MAX_ARGS - 1)
                {
                    args[argc++] = token;
                    token = strtok(NULL, " \t");
                }

                args[argc] = NULL;

                if (argc > 0)
                    execute_command(args);

                exit(0);
            }
            else if (pid > 0)
            {
                close(pipefd[1]);

                char buffer[256];
                ssize_t n;

                while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
                {
                    for (ssize_t x = 0; x < n && j < size - 1; x++)
                        output[j++] = buffer[x];
                }

                close(pipefd[0]);
                waitpid(pid, NULL, 0);

                while (j > 0 && output[j - 1] == '\n')
                    j--;
            }
            else
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }
        }
        else
        {
            output[j++] = input[i++];
        }
    }

    output[j] = '\0';
}

void expand_variables(char *input, char *output, size_t size, int last_status)
{
    size_t i = 0;
    size_t j = 0;

    if (size == 0)
        return;

    while (input[i] != '\0' && j < size - 1)
    {
        if (input[i] == '$')
        {
            i++;

            if (input[i] == '?')
            {
                char status[16];
                snprintf(status, sizeof(status), "%d", last_status);

                size_t length = strlen(status);

                if (length > size - 1 - j)
                    length = size - 1 - j;

                memcpy(output + j, status, length);
                j += length;
                i++;
                continue;
            }

            char variable[128];
            size_t k = 0;

            if (input[i] == '{')
            {
                i++;

                while (input[i] != '\0' && input[i] != '}' && k < sizeof(variable) - 1)
                    variable[k++] = input[i++];

                if (input[i] == '}')
                    i++;
            }
            else
            {
                while ((isalnum((unsigned char)input[i]) || input[i] == '_') && k < sizeof(variable) - 1)
                    variable[k++] = input[i++];
            }

            variable[k] = '\0';

            if (k > 0)
            {
                char *value = getenv(variable);

                if (value != NULL)
                {
                    size_t length = strlen(value);

                    if (length > size - 1 - j)
                        length = size - 1 - j;

                    memcpy(output + j, value, length);
                    j += length;
                }
            }
        }
        else
        {
            output[j++] = input[i++];
        }
    }

    output[j] = '\0';
}


void load_oschrc(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/.oschrc", getenv("HOME"));

    FILE *file = fopen(path, "r");
    if (file == NULL)
        return;

    char line[MAX_COMMAND_LENGTH];

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#')
            continue;

        char *args[MAX_ARGS];
        int i = 0;
        char *token = strtok(line, " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i++] = token;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        if (i > 0)
            execute_command(args);
    }

    fclose(file);
}

int main()
{
    load_oschrc();

    char command[MAX_COMMAND_LENGTH];
    char expanded_command[MAX_COMMAND_LENGTH];
    char *args[MAX_ARGS];
    int last_status = 0;

    while (1)
    {
        
        rl_attempted_completion_function = command_completion;
        char *input_line = readline("ocsh> ");

        if (input_line == NULL)
        {
            printf("\n");
            break;
        }

        add_history(input_line);
        strncpy(command, input_line, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
        free(input_line);
        if (history_count < HISTORY_SIZE)
{
    strcpy(history[history_count], command);
    history_count++;
}

        char command_substituted[MAX_COMMAND_LENGTH];

        expand_command_substitution(command, command_substituted, sizeof(command_substituted));
        expand_variables(command_substituted, expanded_command, sizeof(expanded_command), last_status);
        strcpy(command, expanded_command);

        int i = 0;
        char *token = strtok(command, " \t");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i] = token;
            i++;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        if (i == 0)
            continue;
        if (strcmp(args[0], "past") == 0)
{
    for (int j = 0; j < history_count; j++)
    {
        printf("%d  %s\n", j + 1, history[j]);
    }
    continue;
}

        if (strcmp(args[0], "quit") == 0)
        {
            printf("Goodbye!\n");
            break;
        }

        if (strcmp(args[0], "export") == 0)
        {
            if (args[1] == NULL || strchr(args[1], '=') == NULL)
            {
                printf("Usage: export NOM=valeur\n");
                last_status = 1;
            }
            else
            {
                char *equal = strchr(args[1], '=');
                *equal = '\0';

                if (setenv(args[1], equal + 1, 1) != 0)
                {
                    perror("export");
                    last_status = 1;
                }
                else
                {
                    last_status = 0;
                }
            }

            continue;
        }

        last_status = execute_command(args);
    }

    return 0;
}