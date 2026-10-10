#include "access_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
static char blocklist[MAX_BLOCKLIST][256];
static int blocklist_count = 0;
static int rate_limit_max = 10;
static int rate_limit_window = 60;
typedef struct {
    char client_ip[64];
    int request_count;
    time_t window_start;
    int in_use;
} RateEntry;

static RateEntry rate_table[MAX_TRACKED_CLIENTS];
static int rate_table_count = 0;
static pthread_mutex_t rate_lock = PTHREAD_MUTEX_INITIALIZER;

void access_control_init(const char *config_path) {
    FILE *f = fopen(config_path, "r");
    if (!f) {
        printf("No config file found at %s, using defaults.\n", config_path);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "BLOCK ", 6) == 0 && blocklist_count < MAX_BLOCKLIST) {
            sscanf(line + 6, "%255s", blocklist[blocklist_count]);
            blocklist_count++;
        } else if (strncmp(line, "RATE_LIMIT ", 11) == 0) {
            sscanf(line + 11, "%d/%d", &rate_limit_max, &rate_limit_window);
        }
    }

    fclose(f);
}

int is_blocked(const char *host) {
    for (int i = 0; i < blocklist_count; i++) {
        if (strstr(host, blocklist[i]) != NULL) {
            return 1;   
        }
    }
    return 0;           
}

int is_rate_limited(const char *client_ip) {
    pthread_mutex_lock(&rate_lock);
    time_t now = time(NULL);
    for (int i = 0; i < rate_table_count; i++) {
        if (rate_table[i].in_use && strcmp(rate_table[i].client_ip, client_ip) == 0) {
            if (now - rate_table[i].window_start > rate_limit_window) {
                rate_table[i].window_start = now;
                rate_table[i].request_count = 1;
                pthread_mutex_unlock(&rate_lock);
                return 0;
            }
            rate_table[i].request_count++;
            int limited = rate_table[i].request_count > rate_limit_max;
            pthread_mutex_unlock(&rate_lock);
            return limited;
        }
    }
    if (rate_table_count < MAX_TRACKED_CLIENTS) {
        strncpy(rate_table[rate_table_count].client_ip, client_ip, sizeof(rate_table[0].client_ip) - 1);
        rate_table[rate_table_count].window_start = now;
        rate_table[rate_table_count].request_count = 1;
        rate_table[rate_table_count].in_use = 1;
        rate_table_count++;
    }

    pthread_mutex_unlock(&rate_lock);
    return 0;  
}