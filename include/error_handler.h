#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

/* Sends a minimal HTTP error response to the connected client socket. */

int send_error_response(
    int client_socket,
    int status_code,
    const char *message
);

#endif
