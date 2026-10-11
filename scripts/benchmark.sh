#!/usr/bin/env bash
set -u

URL="${1:-http://127.0.0.1:9091/index.html}"
PROXY_HOST="${2:-127.0.0.1}"
PROXY_PORT="${3:-8080}"
PROXY="http://${PROXY_HOST}:${PROXY_PORT}"

echo "======================================"
echo "Proxy Server Performance Measurement"
echo "======================================"
echo "URL   : $URL"
echo "Proxy : $PROXY"
echo

run_direct() {
    echo "--- 1. Direct request (without proxy) ---"
    curl -sS -o /dev/null \
        -w "HTTP=%{http_code}  Time=%{time_total}s  Size=%{size_download} bytes\n" \
        "$URL"
}

run_proxy() {
    local label="$1"
    echo "--- $label ---"
    curl -sS -o /dev/null \
        -x "$PROXY" \
        -w "HTTP=%{http_code}  Time=%{time_total}s  Size=%{size_download} bytes\n" \
        "$URL"
}

run_direct

echo
run_proxy "2. First request through proxy (cache MISS expected)"

echo
run_proxy "3. Second request through proxy (cache HIT/revalidation expected)"

echo
