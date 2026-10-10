#define _POSIX_C_SOURCE 200809L

#include "include/logger.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_logger(const char *filename)
{
    if (filename == NULL) {
        return -1;
    }

    pthread_mutex_lock(&log_mutex);

    log_file = fopen(filename, "a");
    if (log_file == NULL) {
        pthread_mutex_unlock(&log_mutex);
        return -1;
    }

    pthread_mutex_unlock(&log_mutex);
    return 0;
}

void log_request(
    const char *client_ip,
    const char *method,
    const char *url,
    int status_code,
    const char *cache_status,
    double latency_ms)
{
    time_t now;
    struct tm tm_now;
    char timestamp[32];

    if (client_ip == NULL) client_ip = "-";
    if (method == NULL) method = "-";
    if (url == NULL) url = "-";
    if (cache_status == NULL) cache_status = "-";

    pthread_mutex_lock(&log_mutex);

    if (log_file != NULL) {
        now = time(NULL);
        localtime_r(&now, &tm_now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);

        fprintf(
            log_file,
            "[%s] %s %s %s %d %s %.2fms\n",
            timestamp,
            client_ip,
            method,
            url,
            status_code,
            cache_status,
            latency_ms
        );

        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

void close_logger(void)
{
    pthread_mutex_lock(&log_mutex);

    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }

    pthread_mutex_unlock(&log_mutex);
    pthread_mutex_destroy(&log_mutex);
}
