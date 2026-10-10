#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#define MAX_BLOCKLIST 100
#define MAX_TRACKED_CLIENTS 500

void access_control_init(const char *config_path);
int is_blocked(const char *host);
int is_rate_limited(const char *client_ip);

#endif