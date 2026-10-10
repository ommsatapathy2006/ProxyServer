# Project 1: Designing a Proxy Server

> A proxy server is an intelligent intermediary that stands between client and
> destination, intercepting, evaluating, and relaying network traffic —
> optimizing performance through caching, enforcing security through access
> control, and enabling visibility through comprehensive logging, all while
> remaining transparent to the systems it serves.

## Group Details
- **Course**: Computer Networks
- **Team Members**:
  1. [LAKSHYARAJ DASH] — []
  2. [RUDRA PRATAP SAHOO] — [Roll No. 24155648]
  3. [OMM PRAKASH SATAPATHY] — [Roll No. 24155112]
  4. [SURYAKANT BEHERA] — [Roll No. ]

## Why We Chose This Project
- Every website we visit already passes through some form of proxy or CDN, so this is something we use daily without noticing
- It goes beyond theory: real HTTP parsing, HTTPS tunnelling through CONNECT, and messy real-world cases (broken requests, dead servers, timeouts)
- It let us make our own design calls on caching, access control, and concurrency, and reason through why we made them
- What we build is close to what a real backend or networking engineer would ship

## Features

**Core features**
- Multi-client concurrency: one thread per connection, so clients never block each other
- HTTP request parsing and forwarding (GET/POST)
- HTTPS tunnelling via the CONNECT method, so real browsers work
- Caching: in-memory response cache with visible hit/miss behaviour
- Access control: domain blocklist (403 Forbidden) and per-client rate limiting (429 Too Many Requests)
- Logging: timestamp, client IP, method, URL, status, cache hit/miss, latency
- Graceful error handling: malformed request (400), unreachable host (502), timeout (504)
- Performance measurement: latency and throughput with vs without the proxy

**Additional features**
- Conditional cache revalidation: the proxy asks the origin server `If-Modified-Since` instead of blindly expiring entries, and only re-downloads content that changed
- Config file: blocklist and rate-limit settings are read from `proxy.conf` at startup, so no recompiling is needed

## How a Request Flows
- Client connects, and the server hands it to its own thread
- The request is read and parsed (method, host, port, path)
- Rate limit check, then blocklist check (403 or 429 if refused)
- HTTPS (CONNECT): open a tunnel and relay bytes both ways
- HTTP: check the cache; if the entry exists, revalidate with `If-Modified-Since`; otherwise fetch from the origin and store the response
- Response goes back to the client, and one line is written to the log
- Any failure returns the correct error code instead of crashing

## Team Responsibilities
- **LAKSHYARAJ DASH**: core server and concurrency (`server.c`, `main.c`, `Makefile`)
- **RUDRAPRATAP SAHOO**: HTTP parsing, forwarding, CONNECT tunnelling (`http_handler.c`)
- **OMM PRAKASH SATAPATHY**: caching, blocklist, rate limiting, config file (`cache.c`, `access_control.c`, `proxy.conf`)
- **SURYAKANTA BEHERA**: logging, error handling, benchmarks, documentation (`logger.c`, `scripts/`, `docs/`)

## Project Structure
```
proxy-server/
├── Makefile
├── README.md
├── proxy.conf
├── .gitignore
├── include/    common.h, server.h, http_handler.h, cache.h, access_control.h, logger.h
├── src/        main.c, server.c, http_handler.c, cache.c, access_control.c, logger.c
├── scripts/    benchmark.sh, test_requests.sh
├── tests/      test_http_handler.c, test_cache.c
├── logs/       proxy.log (created at runtime)
└── docs/       design-document.md, demo-script.md
```

## Setup and Running
- Requires Ubuntu (or WSL) with these packages:
```bash
sudo apt update
sudo apt install -y build-essential curl apache2-utils
```
- Build and start (always run from the project root):
```bash
make
./proxy
```
- Use it from the command line:
```bash
curl -x http://localhost:8080 http://example.com
curl -x http://localhost:8080 https://example.com
```
- Use it from a browser: set a manual HTTP/HTTPS proxy to `localhost`, port `8080` in the browser's network settings

## Configuration (`proxy.conf`)
- `BLOCK <domain>`: refuse requests to this domain (403)
- `RATE_LIMIT <max>/<seconds>`: allowed requests per client IP, e.g. `10/60`
- Lines starting with `#` are comments
- Restart the proxy after editing; no recompiling is needed

## Testing
- Run the full check script (proxy must be running):
```bash
./scripts/test_requests.sh
```
- Run the cache module on its own: `make test_cache`
- Check the log: `cat logs/proxy.log`

## Performance Measurement
- Run `./scripts/benchmark.sh` (set a high `RATE_LIMIT` in `proxy.conf` first)
- Compared: direct request, proxy cache miss, proxy cache hit, and 100 requests at concurrency 10 using ApacheBench

| Scenario | Latency | Throughput |
|---|---|---|
| Direct (no proxy) | [fill in] | [fill in] |
| Proxy, cache miss | [fill in] | [fill in] |
| Proxy, cache hit | [fill in] | [fill in] |

## Declared AI Usage
- We are using AI as a collaborative tool across the whole project: planning, coding, debugging, testing, and documentation
- Specifically:
  - Planning and structuring the architecture and module breakdown
  - Generating and refining code .
  - Drafting and refining documentation, including this README and the design document
- Everything AI helps generate is reviewed, tested, and understood by the team before it goes in, since we need to explain and defend our design choices during evaluation

## Known Limitations
- Only responses up to about 8 KB are cached, and the cache lives in memory (cleared on restart)
- HTTPS traffic is tunnelled, not decrypted, so HTTPS can be filtered by hostname only (not by URL path) and cannot be cached
- One thread per connection, with no upper limit on simultaneous clients
- Blocklist matching is by substring of the hostname

## Deliverables
- Working proxy server with source code and setup instructions
- Design document (`docs/design-document.md`)
- Live demo: normal forwarding, cache hit vs miss, and an access-control rule in action (`docs/demo-script.md`)
- Performance report comparing latency and throughput with and without the proxy