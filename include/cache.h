#ifndef CACHE_H
#define CACHE_H

#include <time.h>
#include <stddef.h>

#define MAX_CACHE_ENTRIES 200

#ifndef MAX_URL_LEN
#define MAX_URL_LEN 2048
#endif

#define MAX_CONTENT_LEN 65536

typedef struct {
    char url[MAX_URL_LEN];
    char content[MAX_CONTENT_LEN];
    size_t content_len;
    time_t last_fetched;
    int in_use;
} CacheEntry;

void cache_init(void);
int cache_lookup(const char *url, CacheEntry *out_entry);
void cache_store(const char *url, const char *content, size_t content_len);
void format_http_date(time_t t, char *out, size_t out_size);

#endif