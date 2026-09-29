#!/usr/bin/env bash
set -u

# Proxy Server performance test.
# Measures:
#   1) Direct request
#   2) First request through proxy (expected cache MISS)
#   3) Second request through proxy (expected cache HIT/revalidation)
#
# Usage:
#   ./benchmark.sh <URL> <PROXY_HOST> <PROXY_PORT>
#
# Example:
#   ./benchmark.sh http://example.com 127.0.0.1 8080

URL="${1:-http://example.com}"
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
echo "Repeat the test several times for more stable measurements."
echo "Check proxy.log to correlate status, cache status and latency."
