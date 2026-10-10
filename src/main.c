#include "server.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    unsigned long port = SERVER_DEFAULT_PORT;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2)
    {
        char *end = NULL;
        errno = 0;
        port = strtoul(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0' ||
            port == 0 || port > 65535)
        {
            fprintf(stderr, "Invalid port. Use a number from 1 to 65535.\n");
            return EXIT_FAILURE;
        }
    }

    return server_start((unsigned short)port) == 0
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
