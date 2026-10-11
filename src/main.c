#include "server.h"
#include "access_control.h"
#include "logger.h"
#include "cache.h"
#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

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

    mkdir("logs", 0755);
    access_control_init(CONFIG_FILE_PATH);
    cache_init();
    init_logger(LOG_FILE_PATH);

    int result = server_start((unsigned short)port);
    close_logger();

    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
