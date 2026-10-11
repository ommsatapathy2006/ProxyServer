# Proxy Server Demonstration Script

## Step 1: Start the Proxy Server
```bash
./proxy 8080
```

## Step 2: Normal HTTP Forwarding & Cache Miss
In a second terminal:
```bash
curl -i -x http://127.0.0.1:8080 http://example.com/
```
Observe response `200 OK`. Check `logs/proxy.log` for status `MISS`.

## Step 3: Cache Hit Demonstration
Repeat the exact same request:
```bash
curl -i -x http://127.0.0.1:8080 http://example.com/
```
Observe lower response latency and check `logs/proxy.log` for status `HIT`.

## Step 4: HTTPS CONNECT Tunneling
```bash
curl -i -p -x http://127.0.0.1:8080 https://www.google.com/
```
Observe successful encrypted handshake and data transfer.

## Step 5: Domain Blocking (403 Forbidden)
```bash
curl -i -x http://127.0.0.1:8080 http://badsite.com/
```
Observe immediate `403 Forbidden` response.

## Step 6: Rate Limiting (429 Too Many Requests)
```bash
for i in {1..60}; do curl -s -o /dev/null -w "%{http_code}\n" -x http://127.0.0.1:8080 http://example.com/; done
```
Observe `429` status code once rate threshold is exceeded.

## Step 7: Inspect Administrator Logs
```bash
tail -n 20 logs/proxy.log
```
