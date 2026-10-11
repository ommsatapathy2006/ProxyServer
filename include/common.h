#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <time.h>

#define PROXY_PORT          8080
#define LISTEN_BACKLOG      50
#define BUFFER_SIZE         8192
#define UPSTREAM_TIMEOUT_S  10      

#define CONFIG_FILE_PATH    "proxy.conf"
#define LOG_FILE_PATH       "logs/proxy.log"

#define MAX_METHOD_LEN      16
#define MAX_HOST_LEN        256
#define MAX_REQ_URL_LEN     2048
#define MAX_VERSION_LEN     16
#define MAX_IP_LEN          64

#define HTTP_OK                    200
#define HTTP_NOT_MODIFIED          304
#define HTTP_BAD_REQUEST           400
#define HTTP_FORBIDDEN             403
#define HTTP_TOO_MANY_REQUESTS     429
#define HTTP_BAD_GATEWAY           502
#define HTTP_GATEWAY_TIMEOUT       504

#define CACHE_STATUS_HIT    "HIT"
#define CACHE_STATUS_MISS   "MISS"
#define CACHE_STATUS_NONE   "-"     

typedef struct {
    char method[MAX_METHOD_LEN]; 
    char url[MAX_REQ_URL_LEN];   
    char host[MAX_HOST_LEN];     
    int  port;                        
    char path[MAX_REQ_URL_LEN];  
    char version[MAX_VERSION_LEN];    
    char client_ip[MAX_IP_LEN];       
    int  client_fd;                   
} HttpRequest;

#endif