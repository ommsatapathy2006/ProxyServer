#ifndef CACHE_H
#define CACHE_H

#include <time.h>
#include <stddef.h>

#define MAX_CACHE_ENTRIES 200
#define MAX_URL_LEN 512
#define MAX_CONTENT_LEN 8192

// One "slot" in the cache — think of it as one sticky note with a saved webpage on it
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