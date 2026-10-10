PROXY_HOST="localhost"
PROXY_PORT="8080"
RUN_SLOW=0
for arg in "$@"; do
  case "$arg" in
    --slow)     RUN_SLOW=1 ;;
    *[!0-9]*)   PROXY_HOST="$arg" ;;
    *)          PROXY_PORT="$arg" ;;
  esac
done
PROXY="http://${PROXY_HOST}:${PROXY_PORT}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG="$ROOT/logs/proxy.log"
PASS=0
FAIL=0
GREEN="\033[32m"; RED="\033[31m"; BOLD="\033[1m"; RESET="\033[0m"

check() {   
  if [[ "$3" == "$2" ]]; then
    echo -e "  ${GREEN}[PASS]${RESET} $1 (got $3)"
    PASS=$((PASS + 1))
  else
    echo -e "  ${RED}[FAIL]${RESET} $1 (expected $2, got $3)"
    FAIL=$((FAIL + 1))
  fi
}
http_status() {   
  curl -s -o /dev/null -m "${2:-15}" -w "%{http_code}" -x "$PROXY" "$1"
}
raw_status() {   
  exec 3<>"/dev/tcp/${PROXY_HOST}/${PROXY_PORT}" || return 1
  printf "%b" "$1" >&3
  timeout 5 head -n 1 <&3 | tr -d '\r' | awk '{print $2}'
  exec 3<&- 3>&-
}
if ! (exec 3<>"/dev/tcp/${PROXY_HOST}/${PROXY_PORT}") 2>/dev/null; then
  echo "Proxy is not reachable at ${PROXY_HOST}:${PROXY_PORT}. Start it first with ./proxy"
  exit 1
fi
echo -e "${BOLD}Testing proxy at ${PROXY}${RESET}"
echo -e "\n${BOLD}1. Normal HTTP forwarding${RESET}"
check "plain HTTP page loads through the proxy" "200" "$(http_status http://example.com/)"
echo -e "\n${BOLD}2. HTTPS tunnelling (CONNECT)${RESET}"
check "HTTPS page loads through the proxy" "200" "$(http_status https://example.com/ 20)"
echo -e "\n${BOLD}3. Cache: first request MISS, second request HIT${RESET}"
TAG="$RANDOM$RANDOM"
URL="http://example.com/?cachetest=${TAG}"
T1=$(curl -s -o /dev/null -m 15 -w "%{time_total}" -x "$PROXY" "$URL")
T2=$(curl -s -o /dev/null -m 15 -w "%{time_total}" -x "$PROXY" "$URL")
echo "  first request  : ${T1}s"
echo "  second request : ${T2}s"
if [[ -f "$LOG" ]]; then
  L1=$(grep "cachetest=${TAG}" "$LOG" | sed -n '1p')
  L2=$(grep "cachetest=${TAG}" "$LOG" | sed -n '2p')
  check "first request logged as MISS"  "MISS" "$(echo "$L1" | grep -o -E 'MISS|HIT' | head -n1)"
  check "second request logged as HIT"  "HIT"  "$(echo "$L2" | grep -o -E 'MISS|HIT' | head -n1)"
else
  echo "  (skipped log check: $LOG not found)"
fi
echo -e "\n${BOLD}4. Access control: blocklist${RESET}"
check "badsite.com is blocked"      "403" "$(http_status http://badsite.com/)"
check "ads.example.com is blocked"  "403" "$(http_status http://ads.example.com/)"
echo -e "\n${BOLD}5. Error handling${RESET}"
check "malformed request gets 400"       "400" "$(raw_status 'GARBAGE\r\n\r\n')"
check "request with no Host gets 400"    "400" "$(raw_status 'GET / HTTP/1.1\r\n\r\n')"
check "unreachable host gets 502"        "502" "$(http_status http://this-host-does-not-exist.invalid/)"
if [[ "$RUN_SLOW" -eq 1 ]]; then
  echo "  (slow test: waiting for a connection timeout, up to ~30s)"
  check "unresponsive host gets 504"     "504" "$(http_status http://10.255.255.1/ 40)"
fi
echo -e "\n${BOLD}6. Access control: rate limiting${RESET}"
LIMITED=0
for i in $(seq 1 20); do
  code=$(http_status http://example.com/)
  if [[ "$code" == "429" ]]; then LIMITED=1; break; fi
done
check "rapid requests eventually get 429" "1" "$LIMITED"
echo -e "\n${BOLD}Result: ${PASS} passed, ${FAIL} failed${RESET}"
[[ "$FAIL" -eq 0 ]]