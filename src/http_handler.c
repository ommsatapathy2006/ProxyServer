#define _GNU_SOURCE
#include "http_handler.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int g_timeout_sec = HTTP_DEFAULT_TIMEOUT_SEC;

void http_set_timeout(int seconds)
{
    if (seconds > 0)
        g_timeout_sec = seconds;
}

static int send_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p   += n;
        len -= (size_t)n;
    }
    return 0;
}

static void set_timeouts(int fd, int sec)
{
    struct timeval tv = { .tv_sec = sec, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
}

static int appendf(char *buf, size_t cap, size_t *len, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *len, cap - *len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - *len)
        return -1;
    *len += (size_t)n;
    return 0;
}

const char *http_status_text(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 408: return "Request Timeout";
    case 429: return "Too Many Requests";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 504: return "Gateway Timeout";
    default:  return "Error";
    }
}

void http_send_error(int fd, int status)
{
    char body[256], head[384];
    int blen = snprintf(body, sizeof body,
                        "<html><body><h1>%d %s</h1></body></html>\n",
                        status, http_status_text(status));
    int hlen = snprintf(head, sizeof head,
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n\r\n",
                        status, http_status_text(status), blen);
    if (send_all(fd, head, (size_t)hlen) == 0)
        send_all(fd, body, (size_t)blen);
}

static int fail(int client_fd, int upstream_fd, int status, http_response_t *resp)
{
    if (upstream_fd >= 0)
        close(upstream_fd);
    http_send_error(client_fd, status);
    if (resp)
        resp->status_code = status;
    return status;
}

int http_read_request_head(int fd, char *buf, size_t cap,
                           size_t *head_len, size_t *total_len)
{
    size_t n = 0;

    for (;;) {
        if (n >= cap)
            return 400;

        ssize_t r = recv(fd, buf + n, cap - n, 0);
        if (r == 0)
            return n == 0 ? -1 : 400;
        if (r < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return n == 0 ? -1 : 408;
            return -1;
        }

        size_t start = n >= 3 ? n - 3 : 0;
        n += (size_t)r;

        char *end = memmem(buf + start, n - start, "\r\n\r\n", 4);
        if (end) {
            *head_len  = (size_t)(end - buf) + 4;
            *total_len = n;
            return 0;
        }
    }
}

static int is_token_char(unsigned char c)
{
    return isalnum(c) || strchr("!#$%&'*+-.^_`|~", c) != NULL;
}

static int parse_port(const char *s, size_t len)
{
    if (len == 0 || len > 5)
        return -1;
    int v = 0;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)s[i]))
            return -1;
        v = v * 10 + (s[i] - '0');
    }
    return (v >= 1 && v <= 65535) ? v : -1;
}

static int parse_authority(const char *s, size_t len, int default_port,
                           char *host, int *port)
{
    const char *hstart, *rest;
    size_t hlen, restlen;

    if (len == 0)
        return -1;

    if (s[0] == '[') {
        const char *rb = memchr(s, ']', len);
        if (!rb)
            return -1;
        hstart  = s + 1;
        hlen    = (size_t)(rb - hstart);
        rest    = rb + 1;
        restlen = len - (size_t)(rest - s);
        for (size_t i = 0; i < hlen; i++)
            if (!isxdigit((unsigned char)hstart[i]) && hstart[i] != ':' && hstart[i] != '.')
                return -1;
    } else {
        const char *colon = memchr(s, ':', len);
        hstart  = s;
        hlen    = colon ? (size_t)(colon - s) : len;
        rest    = colon ? colon : s + len;
        restlen = len - hlen;
        for (size_t i = 0; i < hlen; i++)
            if (!isalnum((unsigned char)hstart[i]) && hstart[i] != '.' &&
                hstart[i] != '-' && hstart[i] != '_')
                return -1;
    }

    if (hlen == 0 || hlen >= MAX_HOST_LEN)
        return -1;

    if (restlen == 0) {
        *port = default_port;
    } else {
        if (rest[0] != ':')
            return -1;
        int p = parse_port(rest + 1, restlen - 1);
        if (p < 0)
            return -1;
        *port = p;
    }
    memcpy(host, hstart, hlen);
    host[hlen] = '\0';
    return 0;
}

const char *http_get_header(const http_request_t *req, const char *name)
{
    for (int i = 0; i < req->header_count; i++)
        if (strcasecmp(req->headers[i].name, name) == 0)
            return req->headers[i].value;
    return NULL;
}

static int parse_content_length(const char *v, long *out)
{
    size_t n = strlen(v);
    if (n == 0 || n > 18)
        return -1;
    long x = 0;
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)v[i]))
            return -1;
        x = x * 10 + (v[i] - '0');
    }
    *out = x;
    return 0;
}

static int resolve_target(const char *target, http_request_t *req)
{
    size_t tlen = strlen(target);

    if (req->is_connect) {

        if (parse_authority(target, tlen, 443, req->host, &req->port) != 0)
            return 400;
        req->path[0] = '\0';
        return 0;
    }

    if (strncasecmp(target, "http://", 7) == 0) {

        const char *auth = target + 7;
        size_t alen = strcspn(auth, "/?");
        const char *rest = auth + alen;

        if (memchr(auth, '@', alen))
            return 400;
        if (parse_authority(auth, alen, 80, req->host, &req->port) != 0)
            return 400;

        int n = (*rest == '\0') ? snprintf(req->path, MAX_PATH_LEN, "/")
              : (*rest == '?')  ? snprintf(req->path, MAX_PATH_LEN, "/%s", rest)
                                : snprintf(req->path, MAX_PATH_LEN, "%s", rest);
        return (n < 0 || n >= MAX_PATH_LEN) ? 400 : 0;
    }

    if (target[0] == '/') {

        const char *h = http_get_header(req, "Host");
        if (!h || parse_authority(h, strlen(h), 80, req->host, &req->port) != 0)
            return 400;
        int n = snprintf(req->path, MAX_PATH_LEN, "%s", target);
        return (n < 0 || n >= MAX_PATH_LEN) ? 400 : 0;
    }

    return 400;
}

int http_parse_request(const char *buf, size_t len, http_request_t *req)
{
    memset(req, 0, sizeof *req);
    req->content_length = -1;

    if (len < 4 || memcmp(buf + len - 4, "\r\n\r\n", 4) != 0)
        return 400;
    if (memchr(buf, '\0', len))
        return 400;

    const char *region_end = buf + len - 2;
    const char *eol = memmem(buf, (size_t)(region_end - buf), "\r\n", 2);
    if (!eol)
        return 400;

    char line[MAX_URL_LEN + 64];
    size_t ll = (size_t)(eol - buf);
    if (ll == 0 || ll >= sizeof line)
        return 400;
    memcpy(line, buf, ll);
    line[ll] = '\0';

    char *target = strchr(line, ' ');
    if (!target)
        return 400;
    *target++ = '\0';
    char *version = strchr(target, ' ');
    if (!version)
        return 400;
    *version++ = '\0';
    if (strchr(version, ' '))
        return 400;

    const char *method = line;
    size_t mlen = strlen(method);
    if (mlen == 0 || mlen >= MAX_METHOD_LEN)
        return 400;
    for (size_t i = 0; i < mlen; i++)
        if (!is_token_char((unsigned char)method[i]))
            return 400;
    if (strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0)
        return 400;
    if (target[0] == '\0' || strlen(target) >= MAX_URL_LEN)
        return 400;

    memcpy(req->method, method, mlen + 1);
    memcpy(req->version, version, strlen(version) + 1);
    memcpy(req->url, target, strlen(target) + 1);

    const char *p = eol + 2;
    while (p < region_end) {
        const char *q = memmem(p, (size_t)(region_end - p), "\r\n", 2);
        if (!q)
            return 400;
        if (*p == ' ' || *p == '\t')
            return 400;
        if (req->header_count >= MAX_HEADERS)
            return 400;

        const char *colon = memchr(p, ':', (size_t)(q - p));
        if (!colon || colon == p)
            return 400;

        size_t nlen = (size_t)(colon - p);
        if (nlen >= MAX_HEADER_NAME)
            return 400;
        for (size_t i = 0; i < nlen; i++)
            if (!is_token_char((unsigned char)p[i]))
                return 400;

        const char *v = colon + 1;
        while (v < q && (*v == ' ' || *v == '\t'))
            v++;
        const char *ve = q;
        while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t'))
            ve--;
        size_t vlen = (size_t)(ve - v);
        if (vlen >= MAX_HEADER_VALUE)
            return 400;

        http_header_t *h = &req->headers[req->header_count++];
        memcpy(h->name, p, nlen);
        h->name[nlen] = '\0';
        memcpy(h->value, v, vlen);
        h->value[vlen] = '\0';

        p = q + 2;
    }

    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, "Content-Length") != 0)
            continue;
        long cl;
        if (parse_content_length(req->headers[i].value, &cl) != 0)
            return 400;
        if (req->content_length >= 0 && req->content_length != cl)
            return 400;
        req->content_length = cl;
    }

    req->is_connect = (strcmp(req->method, "CONNECT") == 0);
    if (!req->is_connect && strcmp(req->method, "GET") != 0 &&
        strcmp(req->method, "POST") != 0 && strcmp(req->method, "HEAD") != 0)
        return 501;
    if (!req->is_connect && http_get_header(req, "Transfer-Encoding"))
        return 501;

    return resolve_target(req->url, req);
}

int http_connect_upstream(const char *host, int port, int *err_status)
{
    struct addrinfo hints, *res = NULL, *ai;
    char portstr[8];
    int fd = -1, status = 502;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof portstr, "%d", port);

    if (getaddrinfo(host, portstr, &hints, &res) != 0) {
        *err_status = 502;
        return -1;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            status = 502;
            continue;
        }

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc < 0 && errno == EINPROGRESS) {
            struct pollfd pfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
            int pr;
            do {
                pr = poll(&pfd, 1, g_timeout_sec * 1000);
            } while (pr < 0 && errno == EINTR);

            if (pr == 0) {
                status = 504;
                close(fd);
                fd = -1;
                continue;
            }
            int soerr = 0;
            socklen_t sl = sizeof soerr;
            if (pr < 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0)
                soerr = errno ? errno : ECONNREFUSED;
            if (soerr != 0) {
                status = (soerr == ETIMEDOUT) ? 504 : 502;
                close(fd);
                fd = -1;
                continue;
            }
            rc = 0;
        }

        if (rc == 0) {
            fcntl(fd, F_SETFL, flags);
            set_timeouts(fd, g_timeout_sec);
            break;
        }
        status = 502;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    if (fd < 0)
        *err_status = status;
    return fd;
}

static int skip_header(const char *name)
{
    static const char *const skip[] = {
        "Host", "Connection", "Proxy-Connection", "Proxy-Authorization",
        "Proxy-Authenticate", "Keep-Alive", "TE", "Trailer",
        "Transfer-Encoding", "Upgrade", "Expect", NULL
    };
    for (int i = 0; skip[i]; i++)
        if (strcasecmp(name, skip[i]) == 0)
            return 1;
    return 0;
}

int http_forward(int client_fd, const http_request_t *req,
                 const char *body_prefix, size_t body_prefix_len,
                 http_response_t *resp)
{
    char out[MAX_REQUEST_HEADER_BYTES * 2];
    char buf[BUFFER_SIZE];
    size_t olen = 0;
    int err = 502;

    int up = http_connect_upstream(req->host, req->port, &err);
    if (up < 0)
        return fail(client_fd, -1, err, resp);

    int v6 = strchr(req->host, ':') != NULL;
    char hosthdr[MAX_HOST_LEN + 16];
    if (req->port == 80)
        snprintf(hosthdr, sizeof hosthdr, v6 ? "[%s]" : "%s", req->host);
    else
        snprintf(hosthdr, sizeof hosthdr, v6 ? "[%s]:%d" : "%s:%d", req->host, req->port);

    int bad = appendf(out, sizeof out, &olen, "%s %s HTTP/1.1\r\nHost: %s\r\n",
                      req->method, req->path, hosthdr);
    for (int i = 0; !bad && i < req->header_count; i++) {
        if (skip_header(req->headers[i].name))
            continue;
        bad = appendf(out, sizeof out, &olen, "%s: %s\r\n",
                      req->headers[i].name, req->headers[i].value);
    }

    if (!bad)
        bad = appendf(out, sizeof out, &olen, "Connection: close\r\n\r\n");
    if (bad)
        return fail(client_fd, up, 400, resp);

    if (send_all(up, out, olen) < 0)
        return fail(client_fd, up, 502, resp);

    long remaining = req->content_length > 0 ? req->content_length : 0;
    size_t take = body_prefix_len;
    if ((long)take > remaining)
        take = (size_t)remaining;
    if (take > 0 && send_all(up, body_prefix, take) < 0)
        return fail(client_fd, up, 502, resp);
    remaining -= (long)take;

    if (remaining > 0) {
        const char *exp = http_get_header(req, "Expect");
        if (exp && strcasecmp(exp, "100-continue") == 0)
            send_all(client_fd, "HTTP/1.1 100 Continue\r\n\r\n", 25);
    }
    while (remaining > 0) {
        size_t want = remaining < (long)sizeof buf ? (size_t)remaining : sizeof buf;
        ssize_t r = recv(client_fd, buf, want, 0);
        if (r == 0)
            return fail(client_fd, up, 400, resp);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return fail(client_fd, up,
                        (errno == EAGAIN || errno == EWOULDBLOCK) ? 408 : 400, resp);
        }
        if (send_all(up, buf, (size_t)r) < 0)
            return fail(client_fd, up, 502, resp);
        remaining -= r;
    }

    size_t got = 0;
    int timed_out = 0;
    while (got < 12) {
        ssize_t r = recv(up, buf + got, sizeof buf - got, 0);
        if (r > 0) {
            got += (size_t)r;
        } else if (r == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            timed_out = (errno == EAGAIN || errno == EWOULDBLOCK);
            break;
        }
    }
    if (got < 12 || strncmp(buf, "HTTP/1.", 7) != 0 || buf[8] != ' ' ||
        !isdigit((unsigned char)buf[9]) || !isdigit((unsigned char)buf[10]) ||
        !isdigit((unsigned char)buf[11]))
        return fail(client_fd, up, (got == 0 && timed_out) ? 504 : 502, resp);

    int status = (buf[9] - '0') * 100 + (buf[10] - '0') * 10 + (buf[11] - '0');
    if (resp) {
        resp->status_code   = status;
        resp->bytes_relayed = 0;
    }

    int client_ok = (send_all(client_fd, buf, got) == 0);
    if (client_ok && resp)
        resp->bytes_relayed += got;

    while (client_ok) {
        ssize_t r = recv(up, buf, sizeof buf, 0);
        if (r == 0)
            break;
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (send_all(client_fd, buf, (size_t)r) < 0)
            break;
        if (resp)
            resp->bytes_relayed += (size_t)r;
    }

    close(up);
    return status;
}

int http_tunnel(int client_fd, const http_request_t *req,
                const char *leftover, size_t leftover_len,
                http_response_t *resp)
{
    static const char ok[] = "HTTP/1.1 200 Connection Established\r\n\r\n";
    char buf[BUFFER_SIZE];
    size_t total = 0;
    int err = 502;

    int up = http_connect_upstream(req->host, req->port, &err);
    if (up < 0)
        return fail(client_fd, -1, err, resp);

    if (send_all(client_fd, ok, sizeof ok - 1) < 0) {
        close(up);
        return 0;
    }
    if (leftover_len > 0 && send_all(up, leftover, leftover_len) < 0) {
        close(up);
        return fail(client_fd, -1, 502, resp);
    }
    if (resp)
        resp->status_code = 200;

    int open_c2u = 1, open_u2c = 1;
    while (open_c2u || open_u2c) {
        struct pollfd p[2] = {
            { .fd = open_c2u ? client_fd : -1, .events = POLLIN, .revents = 0 },
            { .fd = open_u2c ? up        : -1, .events = POLLIN, .revents = 0 },
        };
        int pr = poll(p, 2, HTTP_TUNNEL_IDLE_SEC * 1000);
        if (pr < 0 && errno == EINTR)
            continue;
        if (pr <= 0)
            break;

        if (open_c2u && (p[0].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t r = recv(client_fd, buf, sizeof buf, 0);
            if (r > 0) {
                if (send_all(up, buf, (size_t)r) < 0)
                    break;
            } else if (r == 0) {
                shutdown(up, SHUT_WR);
                open_c2u = 0;
            } else if (errno != EINTR && errno != EAGAIN) {
                break;
            }
        }

        if (open_u2c && (p[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t r = recv(up, buf, sizeof buf, 0);
            if (r > 0) {
                if (send_all(client_fd, buf, (size_t)r) < 0)
                    break;
                total += (size_t)r;
            } else if (r == 0) {
                shutdown(client_fd, SHUT_WR);
                open_u2c = 0;
            } else if (errno != EINTR && errno != EAGAIN) {
                break;
            }
        }
    }

    close(up);
    if (resp)
        resp->bytes_relayed = total;
    return 200;
}

int http_handle_client(int client_fd, http_response_t *resp)
{
    http_response_t local;
    if (!resp)
        resp = &local;
    memset(resp, 0, sizeof *resp);

    set_timeouts(client_fd, g_timeout_sec);

    char buf[MAX_REQUEST_HEADER_BYTES];
    size_t head_len = 0, total_len = 0;

    int rc = http_read_request_head(client_fd, buf, sizeof buf, &head_len, &total_len);
    if (rc < 0)
        return 0;
    if (rc != 0) {
        http_send_error(client_fd, rc);
        resp->status_code = rc;
        return rc;
    }

    http_request_t req;
    int perr = http_parse_request(buf, head_len, &req);
    if (perr != 0) {
        http_send_error(client_fd, perr);
        resp->status_code = perr;
        return perr;
    }

    const char *extra = buf + head_len;
    size_t extra_len  = total_len - head_len;

    return req.is_connect
        ? http_tunnel(client_fd, &req, extra, extra_len, resp)
        : http_forward(client_fd, &req, extra, extra_len, resp);
}