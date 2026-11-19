#include <stdio.h>
#include <stdlib.h>

int main()
{
    char command[100];

    while (1)
    {
        printf("ocsh> ");

        if (fgets(command, sizeof(command), stdin) == NULL)
            break;

        if (command[0] == '\n')
            continue;

        if (command[0] == 'e' &&
            command[1] == 'x' &&
            command[2] == 'i' &&
            command[3] == 't')
        {
            printf("Goodbye!\n");
            break;
        }

        printf("Commande reçue : %s", command);
    }

    return 0;
}