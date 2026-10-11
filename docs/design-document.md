# Proxy Server Design Document

## 1. Architecture Overview
The proxy server acts as an intermediary between clients and destination servers.

```
Client <---> [Proxy Listener] ---> [Worker Thread] <---> Destination Server
                                            |
                         +------------------+------------------+
                         |                  |                  |
                   Access Control         Cache             Logger
                   (Block / Rate)     (Store / Reval)    (proxy.log)
```

## 2. Concurrency Model
- Implements thread-per-connection using POSIX threads (`pthread_create`).
- Threads are detached with `pthread_detach()` so resources are automatically reclaimed upon termination.
- All shared resources (cache table, rate limiting table, and logger file descriptor) are protected by dedicated `pthread_mutex_t` locks.

## 3. Protocol Handling
- **HTTP Forwarding**: Parses request line and headers, strips hop-by-hop headers (`Proxy-Connection`, `Keep-Alive`), forwards to destination, and relays responses.
- **HTTPS Tunneling**: Intercepts `CONNECT host:port`, connects to the remote host, replies `200 Connection Established`, and relays raw encrypted bytes bidirectionally using `poll()`.

## 4. Caching and Conditional Revalidation
- Responses for HTTP `GET` requests are cached in a hash/array table keyed by full URL.
- When a cached URL is requested again, the proxy issues a lightweight conditional request using `If-Modified-Since` containing the cached timestamp.
- If origin replies with `304 Not Modified`, the proxy immediately serves the cached body (`CACHE HIT`), saving bandwidth and reducing latency.
- If origin replies with `200 OK`, the cache is updated with the fresh payload (`CACHE MISS / REVALIDATED`).

## 5. Access Control & Error Handling
- **Domain Blocking**: URLs and hostnames matched against the blocklist configured in `proxy.conf` immediately return `403 Forbidden`.
- **Rate Limiting**: Sliding time window tracks per-client IP request counts. Requests exceeding the threshold return `429 Too Many Requests`.
- **Fault Tolerance**: Malformed requests return `400 Bad Request`, unreachable destinations return `502 Bad Gateway`, and timeouts return `504 Gateway Timeout`.
