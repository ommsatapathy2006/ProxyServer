# HTTP/HTTPS Caching Proxy Server

High-performance, multi-threaded caching proxy server built for Ubuntu/Linux.

## Features
- Multi-client concurrency with thread-per-connection handling (`pthread`).
- HTTP request parsing and forwarding (`GET`, `POST`, `HEAD`).
- HTTPS tunneling (`CONNECT` method) with bi-directional streaming via `poll()`.
- Thread-safe in-memory cache with conditional revalidation (`If-Modified-Since` / `304 Not Modified`).
- Access control: domain blocklist (`403 Forbidden`) and per-client sliding window rate limiting (`429 Too Many Requests`).
- Thread-safe structured logging (`logs/proxy.log`).
- Graceful shutdown on `SIGINT` / `SIGTERM`.

## Compilation
```bash
make clean
make all
```

## Running the Proxy
```bash
./proxy 8080
```

## Running Tests
```bash
make test
./scripts/test_requests.sh
```

## Benchmarking
```bash
./scripts/benchmark.sh http://127.0.0.1:9091/index.html 127.0.0.1 8080
```
