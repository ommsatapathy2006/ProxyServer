#include "cache.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
static CacheEntry cache[MAX_CACHE_ENTRIES];
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
void cache_init(void) {
    pthread_mutex_lock(&cache_lock);
    memset(cache, 0, sizeof(cache));   
    pthread_mutex_unlock(&cache_lock);
}
int cache_lookup(const char *url, CacheEntry *out_entry) {
    pthread_mutex_lock(&cache_lock);
for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache[i].in_use && strcmp(cache[i].url, url) == 0) {
            *out_entry = cache[i];           
            pthread_mutex_unlock(&cache_lock);
            return 1;                       
        }
    }
pthread_mutex_unlock(&cache_lock);
    return 0;                                
}

void cache_store(const char *url, const char *content, size_t content_len) {
    pthread_mutex_lock(&cache_lock);

    int target_index = -1;
    time_t oldest_time = 0;
    int oldest_index = 0;

    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache[i].in_use && strcmp(cache[i].url, url) == 0) {
            target_index = i;                
            break;
        }
        if (!cache[i].in_use) {
            target_index = i;               
            break;
        }
        if (oldest_time == 0 || cache[i].last_fetched < oldest_time) {
            oldest_time = cache[i].last_fetched;
            oldest_index = i;
        }
    }

    if (target_index == -1) {
        target_index = oldest_index;        
    }

 strncpy(cache[target_index].url, url, MAX_URL_LEN - 1);
    cache[target_index].url[MAX_URL_LEN - 1] = '\0';

    size_t copy_len = content_len < MAX_CONTENT_LEN - 1 ? content_len : MAX_CONTENT_LEN - 1;
    memcpy(cache[target_index].content, content, copy_len);
    cache[target_index].content_len = copy_len;

    cache[target_index].last_fetched = time(NULL);
    cache[target_index].in_use = 1;

    pthread_mutex_unlock(&cache_lock);
}
void format_http_date(time_t t, char *out, size_t out_size) {
    struct tm *gmt = gmtime(&t);
    strftime(out, out_size, "%a, %d %b %Y %H:%M:%S GMT", gmt);
}