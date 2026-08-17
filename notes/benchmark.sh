#!/usr/bin/env bash
#
# benchmark.sh — run the v1-vs-v2 SET/GET benchmark matrix and dump the results.
#
# For every (server x command x concurrency x pipeline) cell it runs a discarded
# warm-up followed by a measured run of valkey-benchmark, records throughput and
# p50 latency, and classifies the outcome as:
#   ok     - completed normally
#   crash  - the server process died during the run (SIGSEGV/SIGABRT/...)
#   hang   - the run exceeded CELL_TIMEOUT and was killed
#   error  - completed but no result line could be parsed
# A crash restarts the server before the next cell, so one bad cell does not
# poison the rest of the matrix.
#
# Output goes to notes/results/ as both CSV and JSON (plus latest.{csv,json}
# symlinks) and a run-metadata file. Servers run one at a time; v1 listens on
# 6379 (hard-coded), v2 on 6380.
#
# Usage:
#   ./notes/benchmark.sh [--quick] [--format csv|json|both]
#                        [--only v1|v2] [--timeout SECONDS]
#
set -uo pipefail

# ---- Layout -----------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/results"

V1_BIN="$ROOT/build/v1/release/my_redis_server"
V2_BIN="$ROOT/build/v2/release/my_redis_server"
V1_PORT=6379          # v1 hard-codes this; not configurable via CLI
V2_PORT=6380
HOST=127.0.0.1

# ---- Benchmark parameters ---------------------------------------------------
VALUE_SIZE=3                 # -d: value payload in bytes
CONCURRENCY=(1 10 50)        # -c
PIPELINE=(1 16)              # -P
N_PLAIN=50000                # measured ops when P=1
N_PIPE=200000                # measured ops when P>1
WARMUP_DIV=4                 # warm-up ops = measured / WARMUP_DIV
CELL_TIMEOUT=60              # seconds before a run is called a hang

FORMAT=both                  # csv | json | both
ONLY=""                      # "", v1, or v2

# ---- CLI --------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --quick)   N_PLAIN=10000; N_PIPE=40000; shift ;;
    --format)  FORMAT="$2"; shift 2 ;;
    --only)    ONLY="$2"; shift 2 ;;
    --timeout) CELL_TIMEOUT="$2"; shift 2 ;;
    -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

command -v valkey-benchmark >/dev/null 2>&1 || {
  echo "error: valkey-benchmark not found on PATH" >&2; exit 1; }

mkdir -p "$OUT_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
CSV="$OUT_DIR/benchmark-$STAMP.csv"
JSON="$OUT_DIR/benchmark-$STAMP.json"
META="$OUT_DIR/benchmark-$STAMP.meta.txt"

# ROWS holds pipe-delimited records collected during the run; CSV/JSON are
# rendered from it at the end.
ROWS=()
SERVER_PID=""

# ---- Server lifecycle -------------------------------------------------------
port_open() { (exec 3<>"/dev/tcp/$HOST/$1") 2>/dev/null && { exec 3>&- 3<&-; return 0; }; return 1; }

server_alive() { [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; }

start_server() { # name
  local name="$1"
  if [[ "$name" == v1 ]]; then
    "$V1_BIN" >"$OUT_DIR/$name-server.log" 2>&1 &
  else
    "$V2_BIN" -p "$V2_PORT" -s 0 >"$OUT_DIR/$name-server.log" 2>&1 &
  fi
  SERVER_PID=$!
}

stop_server() {
  [[ -n "$SERVER_PID" ]] || return 0
  kill "$SERVER_PID" 2>/dev/null
  wait "$SERVER_PID" 2>/dev/null
  SERVER_PID=""
}

wait_listening() { # port
  local tries=100
  while (( tries-- > 0 )); do
    port_open "$1" && return 0
    sleep 0.1
  done
  return 1
}

ensure_up() { # name port  -> restart if not listening; return 1 if it won't come up
  server_alive && port_open "$2" && return 0
  stop_server
  start_server "$1"
  wait_listening "$2" || { echo "  ! $1 failed to start on $2" >&2; return 1; }
}

trap 'stop_server' EXIT INT TERM

# ---- One measured cell ------------------------------------------------------
measure() { # name port cmd c p   -> "rps|p50|status"
  local name="$1" port="$2" cmd="$3" c="$4" p="$5"
  local n warm; if (( p > 1 )); then n=$N_PIPE; else n=$N_PLAIN; fi
  warm=$(( n / WARMUP_DIV )); (( warm < 1 )) && warm=1

  server_alive || { echo "||crash"; return; }

  # Warm-up (discarded). A hang here still counts against the cell below.
  timeout "$CELL_TIMEOUT" valkey-benchmark -h "$HOST" -p "$port" -d "$VALUE_SIZE" \
    -q -t "$cmd" -n "$warm" -c "$c" -P "$p" >/dev/null 2>&1

  server_alive || { echo "||crash"; return; }

  local out rc
  out="$(timeout "$CELL_TIMEOUT" valkey-benchmark -h "$HOST" -p "$port" -d "$VALUE_SIZE" \
        -q -t "$cmd" -n "$n" -c "$c" -P "$p" 2>/dev/null)"
  rc=$?

  server_alive || { echo "||crash"; return; }
  (( rc == 124 )) && { echo "||hang"; return; }

  # valkey-benchmark -q streams progress on one line separated by \r and ends
  # with a summary like "SET: 21786.49 requests per second, p50=0.039 msec".
  # Split on \r so the summary is its own line, then match it.
  local line rps p50
  line="$(printf '%s\n' "$out" | tr '\r' '\n' | grep -iE "^${cmd}:.*requests per second" | tail -1)"
  rps="$(sed -n 's/.*: \([0-9.]*\) requests per second.*/\1/p' <<<"$line")"
  p50="$(sed -n 's/.*p50=\([0-9.]*\).*/\1/p' <<<"$line")"
  [[ -z "$rps" ]] && { echo "||error"; return; }
  echo "${rps}|${p50}|ok"
}

# ---- Drive one server through the matrix ------------------------------------
bench_server() { # name port
  local name="$1" port="$2"
  echo "== $name =="
  for c in "${CONCURRENCY[@]}"; do
    for p in "${PIPELINE[@]}"; do
      for cmd in set get; do          # valkey-benchmark test names are lowercase
        local CMD="${cmd^^}"          # uppercase for display/CSV/JSON
        local n; if (( p > 1 )); then n=$N_PIPE; else n=$N_PLAIN; fi
        if ! ensure_up "$name" "$port"; then
          ROWS+=("$name|$CMD|$c|$p|$n|||down")
          printf '  c=%-2d P=%-2d %-3s  %s\n' "$c" "$p" "$CMD" "server-down"
          continue
        fi
        local res rps p50 status
        res="$(measure "$name" "$port" "$cmd" "$c" "$p")"
        IFS='|' read -r rps p50 status <<<"$res"
        ROWS+=("$name|$CMD|$c|$p|$n|$rps|$p50|$status")
        printf '  c=%-2d P=%-2d %-3s  %-6s %s%s\n' "$c" "$p" "$CMD" \
          "$status" "${rps:+$rps rps}" "${p50:+  p50=${p50}ms}"
      done
    done
  done
  stop_server
}

# ---- Run --------------------------------------------------------------------
[[ -x "$V1_BIN" ]] || { [[ "$ONLY" == v2 ]] || echo "warn: $V1_BIN missing; skipping v1" >&2; }
[[ -x "$V2_BIN" ]] || { [[ "$ONLY" == v1 ]] || echo "warn: $V2_BIN missing; skipping v2" >&2; }

if [[ "$ONLY" != v2 && -x "$V1_BIN" ]]; then bench_server v1 "$V1_PORT"; fi
if [[ "$ONLY" != v1 && -x "$V2_BIN" ]]; then bench_server v2 "$V2_PORT"; fi

# ---- Metadata ---------------------------------------------------------------
{
  echo "timestamp:   $(date -Is)"
  echo "host:        $(uname -n)"
  echo "cpu:         $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ *//')"
  echo "cores:       $(nproc)"
  echo "kernel:      $(uname -sr)"
  echo "tool:        $(valkey-benchmark --version 2>/dev/null | head -1)"
  echo "git_commit:  $(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "value_size:  ${VALUE_SIZE}B"
  echo "n_plain:     $N_PLAIN"
  echo "n_pipe:      $N_PIPE"
  echo "cell_timeout:${CELL_TIMEOUT}s"
} >"$META"
ln -sf "$(basename "$META")" "$OUT_DIR/latest.meta.txt"

# ---- Emit CSV ---------------------------------------------------------------
write_csv() {
  {
    echo "server,command,concurrency,pipeline,n_ops,rps,p50_ms,status"
    for r in "${ROWS[@]}"; do
      IFS='|' read -r s cmd c p n rps p50 st <<<"$r"
      echo "$s,$cmd,$c,$p,$n,$rps,$p50,$st"
    done
  } >"$CSV"
  ln -sf "$(basename "$CSV")" "$OUT_DIR/latest.csv"
  echo "wrote $CSV"
}

# ---- Emit JSON --------------------------------------------------------------
write_json() {
  {
    echo "{"
    echo "  \"meta\": {"
    local first=1
    while IFS=: read -r k v; do
      v="${v#"${v%%[![:space:]]*}"}"                 # ltrim
      [[ $first -eq 1 ]] && first=0 || echo ","
      printf '    "%s": "%s"' "$(echo "$k" | tr -d ' ')" "${v//\"/\\\"}"
    done <"$META"
    echo ""
    echo "  },"
    echo "  \"results\": ["
    local i
    for i in "${!ROWS[@]}"; do
      IFS='|' read -r s cmd c p n rps p50 st <<<"${ROWS[$i]}"
      printf '    {"server":"%s","command":"%s","concurrency":%s,"pipeline":%s,"n_ops":%s,"rps":%s,"p50_ms":%s,"status":"%s"}' \
        "$s" "$cmd" "$c" "$p" "$n" "${rps:-null}" "${p50:-null}" "$st"
      (( i < ${#ROWS[@]} - 1 )) && echo "," || echo ""
    done
    echo "  ]"
    echo "}"
  } >"$JSON"
  ln -sf "$(basename "$JSON")" "$OUT_DIR/latest.json"
  echo "wrote $JSON"
}

case "$FORMAT" in
  csv)  write_csv ;;
  json) write_json ;;
  both) write_csv; write_json ;;
  *) echo "unknown --format: $FORMAT" >&2; exit 2 ;;
esac
echo "wrote $META"
