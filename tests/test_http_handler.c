#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http_handler.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (cond) g_pass++;                                                  \
        else { g_fail++; printf("    FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

#define TEST(name) printf("[test] %s\n", name)

static http_request_t R;

static int parse(const char *s)
{
    return http_parse_request(s, strlen(s), &R);
}

static void test_parse_absolute_get(void)
{
    TEST("parse: absolute-form GET");
    CHECK(parse("GET http://example.com/index.html?q=1 HTTP/1.1\r\n"
                "Host: example.com\r\nUser-Agent: curl/8\r\n\r\n") == 0);
    CHECK(strcmp(R.method, "GET") == 0);
    CHECK(strcmp(R.host, "example.com") == 0);
    CHECK(R.port == 80);
    CHECK(strcmp(R.path, "/index.html?q=1") == 0);
    CHECK(strcmp(R.version, "HTTP/1.1") == 0);
    CHECK(R.is_connect == 0);
    CHECK(R.content_length == -1);
    CHECK(R.header_count == 2);
}

static void test_parse_port_and_bare_host(void)
{
    TEST("parse: explicit port, empty path, query without path");
    CHECK(parse("GET http://example.com:8080/x HTTP/1.1\r\n\r\n") == 0);
    CHECK(R.port == 8080 && strcmp(R.path, "/x") == 0);

    CHECK(parse("GET http://example.com HTTP/1.1\r\n\r\n") == 0);
    CHECK(strcmp(R.path, "/") == 0);

    CHECK(parse("GET http://example.com?a=b HTTP/1.1\r\n\r\n") == 0);
    CHECK(strcmp(R.path, "/?a=b") == 0);
}

static void test_parse_origin_form(void)
{
    TEST("parse: origin-form uses the Host header");
    CHECK(parse("GET /a/b HTTP/1.1\r\nHost: localhost:9000\r\n\r\n") == 0);
    CHECK(strcmp(R.host, "localhost") == 0 && R.port == 9000);
    CHECK(strcmp(R.path, "/a/b") == 0);

    CHECK(parse("GET /a/b HTTP/1.1\r\nAccept: */*\r\n\r\n") == 400);
}

static void test_parse_connect(void)
{
    TEST("parse: CONNECT");
    CHECK(parse("CONNECT example.com:443 HTTP/1.1\r\nHost: example.com:443\r\n\r\n") == 0);
    CHECK(R.is_connect == 1);
    CHECK(strcmp(R.host, "example.com") == 0 && R.port == 443);

    CHECK(parse("CONNECT [::1]:8443 HTTP/1.1\r\n\r\n") == 0);
    CHECK(strcmp(R.host, "::1") == 0 && R.port == 8443);

    CHECK(parse("CONNECT example.com HTTP/1.1\r\n\r\n") == 0);
    CHECK(R.port == 443);

    CHECK(parse("CONNECT example.com:99999 HTTP/1.1\r\n\r\n") == 400);
    CHECK(parse("CONNECT example.com:abc HTTP/1.1\r\n\r\n") == 400);
    CHECK(parse("CONNECT :443 HTTP/1.1\r\n\r\n") == 400);
}

static void test_parse_headers(void)
{
    TEST("parse: headers, case-insensitive lookup, Content-Length");
    CHECK(parse("POST http://h/p HTTP/1.1\r\ncontent-LENGTH:   12  \r\n"
                "X-Test:a: b\r\n\r\n") == 0);
    CHECK(R.content_length == 12);
    CHECK(http_get_header(&R, "Content-Length") != NULL);
    CHECK(strcmp(http_get_header(&R, "x-test"), "a: b") == 0);
    CHECK(http_get_header(&R, "Missing") == NULL);

    CHECK(parse("POST http://h/ HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\n") == 0);
    CHECK(parse("POST http://h/ HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\n") == 400);
    CHECK(parse("POST http://h/ HTTP/1.1\r\nContent-Length: -1\r\n\r\n") == 400);
    CHECK(parse("POST http://h/ HTTP/1.1\r\nContent-Length: 12abc\r\n\r\n") == 400);
}

static void test_parse_malformed(void)
{
    TEST("parse: malformed input -> 400 (never crashes)");
    CHECK(parse("GARBAGE\r\n\r\n") == 400);
    CHECK(parse("GET http://h/\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/9.9\r\n\r\n") == 400);
    CHECK(parse("GET  http://h/ HTTP/1.1\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/1.1 extra\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/1.1\r\nBadHeaderNoColon\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/1.1\r\n: empty-name\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/1.1\r\nBad Name: x\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/1.1\r\nA: b\r\n folded\r\n\r\n") == 400);
    CHECK(parse("GET http://h/ HTTP/1.1\r\nHost: h\r\n") == 400);
    CHECK(parse("\r\n\r\n") == 400);
    CHECK(parse("GET https://h/ HTTP/1.1\r\n\r\n") == 400);
    CHECK(parse("GET http://us:pw@h/ HTTP/1.1\r\n\r\n") == 400);
    CHECK(parse("GET http://bad host/ HTTP/1.1\r\n\r\n") == 400);
    CHECK(parse("GET hello HTTP/1.1\r\n\r\n") == 400);
    CHECK(http_parse_request("GET http://h/ HTTP/1.1\r\n\r\n", 2, &R) == 400);
    CHECK(http_parse_request("", 0, &R) == 400);

    char nul[] = "GET http://h/ HTTP/1.1\r\nA: b\0c\r\n\r\n";
    CHECK(http_parse_request(nul, sizeof nul - 1, &R) == 400);
}

static void test_parse_unsupported(void)
{
    TEST("parse: unsupported method / chunked upload -> 501");
    CHECK(parse("DELETE http://h/ HTTP/1.1\r\n\r\n") == 501);
    CHECK(parse("PUT http://h/ HTTP/1.1\r\n\r\n") == 501);
    CHECK(parse("POST http://h/ HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") == 501);
    CHECK(parse("HEAD http://h/ HTTP/1.1\r\n\r\n") == 0);
    CHECK(parse("GET http://h/ HTTP/1.0\r\n\r\n") == 0);
}

static void test_parse_limits(void)
{
    TEST("parse: oversized fields are rejected, not overflowed");
    static char big[MAX_REQUEST_HEADER_BYTES * 2];
    int n = snprintf(big, sizeof big, "GET http://h/");
    memset(big + n, 'a', MAX_URL_LEN + 10);
    n += MAX_URL_LEN + 10;
    n += snprintf(big + n, sizeof big - (size_t)n, " HTTP/1.1\r\n\r\n");
    CHECK(http_parse_request(big, (size_t)n, &R) == 400);

    n = snprintf(big, sizeof big, "GET http://h/ HTTP/1.1\r\nX: ");
    memset(big + n, 'b', MAX_HEADER_VALUE + 10);
    n += MAX_HEADER_VALUE + 10;
    n += snprintf(big + n, sizeof big - (size_t)n, "\r\n\r\n");
    CHECK(http_parse_request(big, (size_t)n, &R) == 400);

    n = snprintf(big, sizeof big, "GET http://h/ HTTP/1.1\r\n");
    for (int i = 0; i < MAX_HEADERS + 5; i++)
        n += snprintf(big + n, sizeof big - (size_t)n, "H%d: v\r\n", i);
    n += snprintf(big + n, sizeof big - (size_t)n, "\r\n");
    CHECK(http_parse_request(big, (size_t)n, &R) == 400);
}

typedef struct { int fd; int status; http_response_t resp; } proxy_t;

static void *proxy_thread(void *arg)
{
    proxy_t *p = arg;
    p->status = http_handle_client(p->fd, &p->resp);
    close(p->fd);
    return NULL;
}

static int start_proxy(proxy_t *p, pthread_t *th)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { perror("socketpair"); exit(1); }
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    memset(p, 0, sizeof *p);
    p->fd = sv[0];
    pthread_create(th, NULL, proxy_thread, p);
    return sv[1];
}

static size_t read_all(int fd, char *out, size_t cap)
{
    size_t n = 0;
    while (n < cap - 1) {
        ssize_t r = recv(fd, out + n, cap - 1 - n, 0);
        if (r <= 0) break;
        n += (size_t)r;
    }
    out[n] = '\0';
    return n;
}

static void write_str(int fd, const char *s)
{
    if (send(fd, s, strlen(s), MSG_NOSIGNAL) < 0) perror("send");
}

static int listen_local(int *port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = 0 };
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0 || listen(fd, 4) != 0) {
        perror("bind/listen");
        exit(1);
    }
    socklen_t l = sizeof a;
    getsockname(fd, (struct sockaddr *)&a, &l);
    *port = ntohs(a.sin_port);
    return fd;
}

enum { ORIGIN_HELLO, ORIGIN_ECHO_BODY, ORIGIN_HANG, ORIGIN_ECHO_TUNNEL };
typedef struct { int lfd; int mode; } origin_t;

static char   g_origin_rx[16384];

static void *origin_thread(void *arg)
{
    origin_t *o = arg;
    int fd = accept(o->lfd, NULL, NULL);
    if (fd < 0) return NULL;
    char buf[4096];

    if (o->mode == ORIGIN_ECHO_TUNNEL) {
        ssize_t r;
        while ((r = recv(fd, buf, sizeof buf, 0)) > 0)
            send(fd, buf, (size_t)r, MSG_NOSIGNAL);
        close(fd);
        return NULL;
    }

    size_t n = 0;
    memset(g_origin_rx, 0, sizeof g_origin_rx);
    char *hend = NULL;
    while (n < sizeof g_origin_rx - 1 && !(hend = strstr(g_origin_rx, "\r\n\r\n"))) {
        ssize_t r = recv(fd, g_origin_rx + n, sizeof g_origin_rx - 1 - n, 0);
        if (r <= 0) break;
        n += (size_t)r;
    }
    long cl = 0;
    const char *p = strcasestr(g_origin_rx, "Content-Length:");
    if (p) cl = atol(p + 15);
    while (hend && (long)(n - (size_t)(hend + 4 - g_origin_rx)) < cl && n < sizeof g_origin_rx - 1) {
        ssize_t r = recv(fd, g_origin_rx + n, sizeof g_origin_rx - 1 - n, 0);
        if (r <= 0) break;
        n += (size_t)r;
    }

    if (o->mode == ORIGIN_HANG) {
        while (recv(fd, buf, sizeof buf, 0) > 0) { }
    } else if (o->mode == ORIGIN_ECHO_BODY && hend) {
        char head[128];
        int hl = snprintf(head, sizeof head,
                          "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", cl);
        send(fd, head, (size_t)hl, MSG_NOSIGNAL);
        send(fd, hend + 4, (size_t)cl, MSG_NOSIGNAL);
    } else {
        const char *resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                           "Content-Length: 5\r\nConnection: close\r\n\r\nhello";
        send(fd, resp, strlen(resp), MSG_NOSIGNAL);
    }
    close(fd);
    return NULL;
}

static pthread_t start_origin(origin_t *o, int mode, int *port)
{
    o->lfd  = listen_local(port);
    o->mode = mode;
    pthread_t th;
    pthread_create(&th, NULL, origin_thread, o);
    return th;
}

static void test_forward_get(void)
{
    TEST("e2e: GET is rewritten to origin-form and the reply is relayed");
    int port; origin_t o; pthread_t ot = start_origin(&o, ORIGIN_HELLO, &port);
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);

    char req[512], hostline[64];
    snprintf(req, sizeof req,
             "GET http://127.0.0.1:%d/path?x=1 HTTP/1.1\r\n"
             "Host: 127.0.0.1:%d\r\n"
             "Proxy-Connection: keep-alive\r\n"
             "Proxy-Authorization: Basic secret\r\n"
             "Connection: keep-alive\r\n"
             "User-Agent: unit-test\r\n\r\n", port, port);
    write_str(c, req);

    char rx[4096];
    read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL); pthread_join(ot, NULL);
    close(c); close(o.lfd);

    CHECK(strstr(rx, "HTTP/1.1 200 OK") == rx);
    CHECK(strlen(rx) >= 5 && strcmp(rx + strlen(rx) - 5, "hello") == 0);
    CHECK(p.status == 200);
    CHECK(p.resp.status_code == 200);
    CHECK(p.resp.bytes_relayed == strlen(rx));

    snprintf(hostline, sizeof hostline, "Host: 127.0.0.1:%d\r\n", port);
    CHECK(strncmp(g_origin_rx, "GET /path?x=1 HTTP/1.1\r\n", 24) == 0);
    CHECK(strstr(g_origin_rx, hostline) != NULL);
    CHECK(strstr(g_origin_rx, "User-Agent: unit-test") != NULL);
    CHECK(strstr(g_origin_rx, "Connection: close") != NULL);
    CHECK(strcasestr(g_origin_rx, "Proxy-Connection") == NULL);
    CHECK(strcasestr(g_origin_rx, "Proxy-Authorization") == NULL);
    CHECK(strcasestr(g_origin_rx, "keep-alive") == NULL);
}

static void test_forward_post(int split)
{
    TEST(split ? "e2e: POST with the body arriving after the head"
               : "e2e: POST with head and body in one packet");
    int port; origin_t o; pthread_t ot = start_origin(&o, ORIGIN_ECHO_BODY, &port);
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);

    char head[512];
    snprintf(head, sizeof head,
             "POST http://127.0.0.1:%d/form HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: 10\r\n\r\n", port, port);
    write_str(c, head);
    if (split) usleep(150 * 1000);
    write_str(c, "name=rudra");

    char rx[4096];
    read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL); pthread_join(ot, NULL);
    close(c); close(o.lfd);

    CHECK(strstr(rx, "200 OK") != NULL);
    CHECK(strlen(rx) >= 10 && strcmp(rx + strlen(rx) - 10, "name=rudra") == 0);
    CHECK(strncmp(g_origin_rx, "POST /form HTTP/1.1\r\n", 21) == 0);
    CHECK(strstr(g_origin_rx, "Content-Length: 10") != NULL);
    CHECK(strstr(g_origin_rx, "\r\n\r\nname=rudra") != NULL);
}

static void test_origin_form_via_host_header(void)
{
    TEST("e2e: origin-form request (curl localhost:PORT style)");
    int port; origin_t o; pthread_t ot = start_origin(&o, ORIGIN_HELLO, &port);
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);

    char req[256];
    snprintf(req, sizeof req, "GET /hi HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", port);
    write_str(c, req);

    char rx[1024];
    read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL); pthread_join(ot, NULL);
    close(c); close(o.lfd);

    CHECK(strstr(rx, "200 OK") != NULL);
    CHECK(strncmp(g_origin_rx, "GET /hi HTTP/1.1\r\n", 18) == 0);
}

static void expect_error(const char *label, const char *req, const char *expect, int status)
{
    TEST(label);
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);
    write_str(c, req);
    char rx[1024];
    read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL);
    close(c);
    CHECK(strstr(rx, expect) == rx + 9);
    CHECK(strstr(rx, "Connection: close") != NULL);
    CHECK(p.status == status);
    CHECK(p.resp.status_code == status);
}

static void test_error_responses(void)
{
    expect_error("e2e: malformed request -> 400",
                 "BOGUS\r\n\r\n", "400 Bad Request", 400);
    expect_error("e2e: origin-form without Host -> 400",
                 "GET /x HTTP/1.1\r\n\r\n", "400 Bad Request", 400);
    expect_error("e2e: unsupported method -> 501",
                 "DELETE http://127.0.0.1:1/ HTTP/1.1\r\n\r\n", "501 Not Implemented", 501);
    expect_error("e2e: origin refuses connection -> 502",
                 "GET http://127.0.0.1:1/ HTTP/1.1\r\nHost: 127.0.0.1:1\r\n\r\n",
                 "502 Bad Gateway", 502);
    expect_error("e2e: unresolvable host -> 502",
                 "GET http://no-such-host.invalid/ HTTP/1.1\r\n\r\n", "502 Bad Gateway", 502);
    expect_error("e2e: CONNECT to closed port -> 502",
                 "CONNECT 127.0.0.1:1 HTTP/1.1\r\n\r\n", "502 Bad Gateway", 502);
}

static void test_head_too_large(void)
{
    TEST("e2e: request head larger than the limit -> 400");
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);
    write_str(c, "GET http://h/ HTTP/1.1\r\nX: ");
    char junk[1024];
    memset(junk, 'a', sizeof junk);
    for (int i = 0; i < 20; i++)
        if (send(c, junk, sizeof junk, MSG_NOSIGNAL) < 0) break;
    char rx[512];
    read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL);
    close(c);
    CHECK(strstr(rx, "400 Bad Request") != NULL);
    CHECK(p.status == 400);
}

static void test_silent_client(void)
{
    TEST("e2e: client that connects and sends nothing gets no response");
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);
    shutdown(c, SHUT_WR);
    char rx[64];
    size_t n = read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL);
    close(c);
    CHECK(n == 0);
    CHECK(p.status == 0);
}

static void test_gateway_timeout(void)
{
    TEST("e2e: origin accepts but never answers -> 504");
    http_set_timeout(1);
    int port; origin_t o; pthread_t ot = start_origin(&o, ORIGIN_HANG, &port);
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);

    char req[256];
    snprintf(req, sizeof req,
             "GET http://127.0.0.1:%d/slow HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", port, port);
    write_str(c, req);

    char rx[1024];
    read_all(c, rx, sizeof rx);
    pthread_join(pt, NULL); pthread_join(ot, NULL);
    close(c); close(o.lfd);
    http_set_timeout(HTTP_DEFAULT_TIMEOUT_SEC);

    CHECK(strstr(rx, "504 Gateway Timeout") != NULL);
    CHECK(p.status == 504);
}

static void test_connect_tunnel(void)
{
    TEST("e2e: CONNECT tunnel relays bytes both ways and honours half-close");
    int port; origin_t o; pthread_t ot = start_origin(&o, ORIGIN_ECHO_TUNNEL, &port);
    proxy_t p; pthread_t pt; int c = start_proxy(&p, &pt);

    char req[256];
    snprintf(req, sizeof req,
             "CONNECT 127.0.0.1:%d HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", port, port);
    write_str(c, req);

    char rx[256]; size_t n = 0;
    while (n < sizeof rx - 1) {
        ssize_t r = recv(c, rx + n, 1, 0);
        if (r <= 0) break;
        rx[++n] = '\0';
        if (n >= 4 && strcmp(rx + n - 4, "\r\n\r\n") == 0) break;
    }
    CHECK(strcmp(rx, "HTTP/1.1 200 Connection Established\r\n\r\n") == 0);

    const char msg[] = "\x16\x03\x01 ping-through-tunnel \x00 end";
    send(c, msg, sizeof msg, 0);
    char echo[128]; size_t got = 0;
    while (got < sizeof msg) {
        ssize_t r = recv(c, echo + got, sizeof msg - got, 0);
        if (r <= 0) break;
        got += (size_t)r;
    }
    CHECK(got == sizeof msg && memcmp(echo, msg, sizeof msg) == 0);

    shutdown(c, SHUT_WR);
    char tail[16];
    CHECK(recv(c, tail, sizeof tail, 0) == 0);
    pthread_join(pt, NULL); pthread_join(ot, NULL);
    close(c); close(o.lfd);

    CHECK(p.status == 200);
    CHECK(p.resp.status_code == 200);
    CHECK(p.resp.bytes_relayed == sizeof msg);
}

int main(void)
{
    test_parse_absolute_get();
    test_parse_port_and_bare_host();
    test_parse_origin_form();
    test_parse_connect();
    test_parse_headers();
    test_parse_malformed();
    test_parse_unsupported();
    test_parse_limits();

    test_forward_get();
    test_forward_post(0);
    test_forward_post(1);
    test_origin_form_via_host_header();
    test_error_responses();
    test_head_too_large();
    test_silent_client();
    test_gateway_timeout();
    test_connect_tunnel();

    printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}