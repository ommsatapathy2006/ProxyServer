#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <stddef.h>
#include "common.h"

#define HTTP_DEFAULT_TIMEOUT_SEC  10
#define HTTP_TUNNEL_IDLE_SEC      60

int http_handle_client(int client_fd, http_response_t *resp);

int http_read_request_head(int fd, char *buf, size_t cap,
                           size_t *head_len, size_t *total_len);

int http_parse_request(const char *buf, size_t len, http_request_t *req);

const char *http_get_header(const http_request_t *req, const char *name);

int http_connect_upstream(const char *host, int port, int *err_status);

int http_forward(int client_fd, const http_request_t *req,
                 const char *body_prefix, size_t body_prefix_len,
                 http_response_t *resp);

int http_tunnel(int client_fd, const http_request_t *req,
                const char *leftover, size_t leftover_len,
                http_response_t *resp);

void http_send_error(int fd, int status);
const char *http_status_text(int status);

void http_set_timeout(int seconds);

#endif