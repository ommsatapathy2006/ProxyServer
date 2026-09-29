#ifndef LOGGER_H
#define LOGGER_H

/* Thread-safe request logger for the Proxy Server project. */

int init_logger(const char *filename);

void log_request(
    const char *client_ip,
    const char *method,
    const char *url,
    int status_code,
    const char *cache_status,
    double latency_ms
);

void close_logger(void);

#endif
