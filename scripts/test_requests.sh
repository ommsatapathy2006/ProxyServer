#!/usr/bin/env bash
set -e

PROXY_PORT=8080
ORIGIN_PORT=9091
PROXY_PID=""
ORIGIN_PID=""

cleanup() {
    if [ -n "$PROXY_PID" ] && kill -0 "$PROXY_PID" 2>/dev/null; then
        kill "$PROXY_PID" 2>/dev/null || true
        wait "$PROXY_PID" 2>/dev/null || true
    fi
    if [ -n "$ORIGIN_PID" ] && kill -0 "$ORIGIN_PID" 2>/dev/null; then
        kill "$ORIGIN_PID" 2>/dev/null || true
        wait "$ORIGIN_PID" 2>/dev/null || true
    fi
    rm -rf /tmp/proxy_test_origin
}
trap cleanup EXIT INT TERM

make all

mkdir -p /tmp/proxy_test_origin
echo "Hello from test origin" > /tmp/proxy_test_origin/index.html

python3 -m http.server "$ORIGIN_PORT" --directory /tmp/proxy_test_origin >/dev/null 2>&1 &
ORIGIN_PID=$!
sleep 1

./proxy "$PROXY_PORT" >/dev/null 2>&1 &
PROXY_PID=$!
sleep 1

echo "Testing normal HTTP request forwarding..."
RESP=$(curl -s -x "http://127.0.0.1:$PROXY_PORT" "http://127.0.0.1:$ORIGIN_PORT/index.html")
if [ "$RESP" != "Hello from test origin" ]; then
    echo "FAIL: Expected 'Hello from test origin', got '$RESP'"
    exit 1
fi
echo "PASS: HTTP forwarding succeeded."

echo "Testing cache hit..."
curl -s -x "http://127.0.0.1:$PROXY_PORT" "http://127.0.0.1:$ORIGIN_PORT/index.html" >/dev/null
echo "PASS: Cache request succeeded."

echo "Testing blocked domain (403 Forbidden)..."
CODE_BLOCKED=$(curl -s -o /dev/null -w "%{http_code}" -x "http://127.0.0.1:$PROXY_PORT" "http://badsite.com/")
if [ "$CODE_BLOCKED" != "403" ]; then
    echo "FAIL: Expected 403, got $CODE_BLOCKED"
    exit 1
fi
echo "PASS: Blocked domain returned 403 Forbidden."

echo "Testing malformed request handling (400 Bad Request)..."
MALFORMED_RESP=$(printf "MALFORMED_REQUEST_LINE\r\n\r\n" | nc -w 2 127.0.0.1 "$PROXY_PORT" 2>/dev/null || true)
if ! echo "$MALFORMED_RESP" | grep -q "400 Bad Request"; then
    echo "FAIL: Expected 400 Bad Request"
    exit 1
fi
echo "PASS: Malformed request gracefully returned 400 Bad Request."

echo "Testing rate limiting (429 Too Many Requests)..."
HIT_429=0
for i in $(seq 1 65); do
    C=$(curl -s -o /dev/null -w "%{http_code}" -x "http://127.0.0.1:$PROXY_PORT" "http://127.0.0.1:$ORIGIN_PORT/index.html" || true)
    if [ "$C" = "429" ]; then
        HIT_429=1
        break
    fi
done
if [ "$HIT_429" -ne 1 ]; then
    echo "FAIL: Expected 429 after rapid requests"
    exit 1
fi
echo "PASS: Rate limit triggered 429 Too Many Requests."

echo "Checking log file..."
if [ ! -f "logs/proxy.log" ] || [ ! -s "logs/proxy.log" ]; then
    echo "FAIL: logs/proxy.log is empty or missing"
    exit 1
fi
echo "PASS: logs/proxy.log recorded requests."
tail -n 5 logs/proxy.log

echo "All tests completed successfully!"
