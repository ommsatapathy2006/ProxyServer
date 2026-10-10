#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "http_handler.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct
{
    int client_fd;
    char client_ip[SERVER_IP_SIZE];
} ClientInfo;

static volatile sig_atomic_t shutdown_requested = 0;
static int listening_fd = -1;

/* Signal handlers should do as little as possible. close() is async-signal-safe. */
static void handle_shutdown_signal(int signal_number)
{
    (void)signal_number;
    shutdown_requested = 1;
    if (listening_fd >= 0)
    {
        close(listening_fd);
        listening_fd = -1;
    }
}

static void get_client_ip(const struct sockaddr *address,
                          socklen_t address_length,
                          char *buffer, size_t buffer_size)
{
    int result = getnameinfo(address, address_length, buffer,
                             (socklen_t)buffer_size, NULL, 0, NI_NUMERICHOST);
    if (result != 0)
    {
        snprintf(buffer, buffer_size, "unknown");
    }
}

/* Each thread owns one accepted client socket and closes it when finished. */
void *server_client_thread(void *argument)
{
    ClientInfo *client = (ClientInfo *)argument;
    int client_fd = client->client_fd;
    char client_ip[SERVER_IP_SIZE];

    snprintf(client_ip, sizeof(client_ip), "%s", client->client_ip);
    free(client);

    /* Member B's HTTP/CONNECT handler processes this client connection. */
    (void)handle_http_client(client_fd, client_ip);

    close(client_fd);
    return NULL;
}

int server_start(unsigned short port)
{
    struct sockaddr_in server_address;
    struct sigaction action;
    memset(&server_address, 0, sizeof(server_address));
    memset(&action, 0, sizeof(action));

    action.sa_handler = handle_shutdown_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    listening_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_fd < 0)
    {
        perror("socket");
        return -1;
    }

    int reuse_address = 1;
    if (setsockopt(listening_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_address, sizeof(reuse_address)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR)");
        close(listening_fd);
        listening_fd = -1;
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(listening_fd, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("bind");
        close(listening_fd);
        listening_fd = -1;
        return -1;
    }

    if (listen(listening_fd, SERVER_BACKLOG) < 0)
    {
        perror("listen");
        close(listening_fd);
        listening_fd = -1;
        return -1;
    }

    printf("Proxy server listening on port %u\n", (unsigned int)port);
    printf("Press Ctrl+C to stop.\n");

    while (!shutdown_requested)
    {
        struct sockaddr_storage client_address;
        socklen_t address_length = sizeof(client_address);

        int client_fd = accept(listening_fd,
                               (struct sockaddr *)&client_address,
                               &address_length);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            if (shutdown_requested || errno == EBADF || errno == EINVAL)
                break;
            perror("accept");
            continue;
        }

        ClientInfo *client = (ClientInfo *)calloc(1, sizeof(*client));
        if (client == NULL)
        {
            perror("calloc");
            close(client_fd);
            continue;
        }

        client->client_fd = client_fd;
        get_client_ip((struct sockaddr *)&client_address, address_length,
                      client->client_ip, sizeof(client->client_ip));

        pthread_t thread_id;
        int thread_result = pthread_create(&thread_id, NULL,
                                           server_client_thread, client);
        if (thread_result != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(thread_result));
            close(client_fd);
            free(client);
            continue;
        }

        thread_result = pthread_detach(thread_id);
        if (thread_result != 0)
        {
            fprintf(stderr, "pthread_detach: %s\n", strerror(thread_result));
            /* The thread is already running; do not free its client data here. */
        }
    }

    if (listening_fd >= 0)
    {
        close(listening_fd);
        listening_fd = -1;
    }
    printf("Proxy server stopped cleanly.\n");
    return 0;
}
