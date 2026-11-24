#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "command_substitution.h"
#include "execution.h"

#define MAX_ARGS 64

void expand_command_substitution(char *input, char *output, size_t size)
{
    if (input == NULL || output == NULL || size == 0)
        return;

    size_t i = 0, j = 0;

    while (input[i] != '\0' && j < size - 1)
    {
        /* Ne pas confondre $((...)) avec $(...) */
        if (input[i] == '$' && input[i + 1] == '('
            && input[i + 2] == '(')
        {
            output[j++] = input[i++];
            output[j++] = input[i++];
            output[j++] = input[i++];
            continue;
        }

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

                if (k < sizeof(inner) - 1)
                    inner[k++] = input[i];

                i++;
            }

            inner[k] = '\0';

            int pipefd[2];

            if (pipe(pipefd) == -1)
            {
                output[j] = '\0';
                return;
            }

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

                _exit(0);
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
