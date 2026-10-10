#include <stdio.h>
#include <string.h>
#include "cache.h"

int main() {
    cache_init();
    const char *url = "http://example.com/";
    const char *content = "<html>Hello World</html>";

    printf("Test 1: Looking up before storing anything...\n");
    CacheEntry entry;
    int found = cache_lookup(url, &entry);
    printf("  Found: %s (expected: no)\n\n", found ? "yes" : "no");

    printf("Test 2: Storing the entry...\n");
    cache_store(url, content, strlen(content));
    printf("  Stored.\n\n");

    printf("Test 3: Looking up after storing...\n");
    found = cache_lookup(url, &entry);
    printf("  Found: %s (expected: yes)\n", found ? "yes" : "no");
    if (found) {
        printf("  Content: %s\n", entry.content);

        char date_str[64];
        format_http_date(entry.last_fetched, date_str, sizeof(date_str));
        printf("  Last-fetched as HTTP date: %s\n\n", date_str);
    }

    printf("Test 4: Updating the same URL with new content...\n");
    const char *new_content = "<html>Updated page</html>";
    cache_store(url, new_content, strlen(new_content));
    cache_lookup(url, &entry);
    printf("  New content: %s\n", entry.content);

    return 0;
}