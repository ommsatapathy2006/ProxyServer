#define _POSIX_C_SOURCE 200809L

#include "include/error_handler.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static const char *reason_phrase(int status_code)
{
    switch (status_code) {
        case 400: return "Bad Request";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        default:  return "Proxy Error";
    }
}

int send_error_response(
    int client_socket,
    int status_code,
    const char *message)
{
    char body[512];
    char response[1024];
    int body_len;
    int response_len;

    if (message == NULL) {
        message = reason_phrase(status_code);
    }

    body_len = snprintf(
        body,
        sizeof(body),
        "<html><body><h1>%d %s</h1><p>%s</p></body></html>\\r\\n",
        status_code,
        reason_phrase(status_code),
        message
    );

    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        return -1;
    }

    response_len = snprintf(
        response,
        sizeof(response),
        "HTTP/1.1 %d %s\\r\\n"
        "Content-Type: text/html\\r\\n"
        "Content-Length: %d\\r\\n"
        "Connection: close\\r\\n"
        "\\r\\n"
        "%s",
        status_code,
        reason_phrase(status_code),
        body_len,
        body
    );

    if (response_len < 0 || response_len >= (int)sizeof(response)) {
        return -1;
    }

    return (send(client_socket, response, (size_t)response_len, 0) < 0) ? -1 : 0;
}
