#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <time.h>

#define SERVER_DEFAULT_PORT 8080
#define SERVER_BACKLOG 32
#define SERVER_IP_SIZE 64
#define REQUEST_METHOD_SIZE 16
#define REQUEST_HOST_SIZE 256
#define REQUEST_PORT_SIZE 16
#define REQUEST_PATH_SIZE 2048
#define REQUEST_HEADERS_SIZE 8192
#define RESPONSE_STATUS_SIZE 64
#define RESPONSE_HEADERS_SIZE 8192

/*
 * Shared data structures for the team.
 * These are common definitions for future modules. Member B's current
 * handler may still use its own private parsed-request structure until
 * the team connects the interfaces during integration.
 */
typedef struct
{
    char method[REQUEST_METHOD_SIZE];
    char host[REQUEST_HOST_SIZE];
    char port[REQUEST_PORT_SIZE];
    char path[REQUEST_PATH_SIZE];
    char version[16];
    char headers[REQUEST_HEADERS_SIZE];
} ProxyRequest;

typedef struct
{
    int status_code;
    char status_text[RESPONSE_STATUS_SIZE];
    char headers[RESPONSE_HEADERS_SIZE];
    char *body;
    size_t body_length;
} ProxyResponse;

/* Starts the TCP proxy listener and handles clients until shutdown. */
int server_start(unsigned short port);

/* Called by the per-client thread. Can be replaced with the team's pipeline. */
void *server_client_thread(void *argument);

#endif
