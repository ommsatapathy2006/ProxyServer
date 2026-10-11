#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <stddef.h>
#include "common.h"

#define HTTP_DEFAULT_TIMEOUT_SEC  10
#define HTTP_TUNNEL_IDLE_SEC      60

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN              2048
#endif

#ifndef MAX_URL_LEN
#define MAX_URL_LEN               2048
#endif

#define MAX_HEADERS               100
#define MAX_HEADER_NAME           128
#define MAX_HEADER_VALUE          2048
#define MAX_REQUEST_HEADER_BYTES  8192

typedef struct {
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} http_header_t;

typedef struct {
    char method[MAX_METHOD_LEN];
    char url[MAX_URL_LEN];
    char host[MAX_HOST_LEN];
    int port;
    char path[MAX_PATH_LEN];
    char version[MAX_VERSION_LEN];
    int is_connect;
    long content_length;
    http_header_t headers[MAX_HEADERS];
    int header_count;
} http_request_t;

typedef struct {
    int status_code;
    size_t bytes_relayed;
} http_response_t;

int handle_http_client(int client_fd, const char *client_ip);
int http_handle_client(int client_fd, http_response_t *resp);
int http_read_request_head(int fd, char *buf, size_t cap, size_t *head_len, size_t *total_len);
int http_parse_request(const char *buf, size_t len, http_request_t *req);
const char *http_get_header(const http_request_t *req, const char *name);
int http_connect_upstream(const char *host, int port, int *err_status);
int http_forward(int client_fd, const http_request_t *req, const char *body_prefix, size_t body_prefix_len, http_response_t *resp);
int http_tunnel(int client_fd, const http_request_t *req, const char *leftover, size_t leftover_len, http_response_t *resp);
void http_send_error(int fd, int status);
const char *http_status_text(int status);
void http_set_timeout(int seconds);

#endif