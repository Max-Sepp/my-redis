#!/usr/bin/env bash
#
# lib.sh — shared helpers for v2's end-to-end handler tests.
#
# Each test_<handler>.sh sources this, starts its own my_redis_server instance
# on its own port (see PORT assignments in each test file), speaks raw RESP
# over bash's /dev/tcp, and asserts on the exact reply bytes. Kept dependency-
# free (no redis-cli/valkey required) so it can run anywhere the project
# builds, following the same /dev/tcp + background-process style as
# notes/benchmark.sh.
#
set -uo pipefail

E2E_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$E2E_DIR/../../.." && pwd)"

# Resolve the server binary: honour SERVER_BIN if set, otherwise probe the
# usual CMake/CMakePresets output locations.
if [[ -z "${SERVER_BIN:-}" ]]; then
  for candidate in \
    "$ROOT/build/v2/debug/my_redis_server" \
    "$ROOT/build/v2/release/my_redis_server" \
    "$ROOT/build/v2/my_redis_server" \
    "$ROOT/v2/build/my_redis_server"; do
    if [[ -x "$candidate" ]]; then
      SERVER_BIN="$candidate"
      break
    fi
  done
fi

HOST=127.0.0.1
SERVER_PID=""
SERVER_LOG="$(mktemp)"

PASS=0
FAIL=0

port_open() {
  (exec 3<>"/dev/tcp/$HOST/$1") 2>/dev/null && { exec 3>&- 3<&-; return 0; }
  return 1
}

# start_server <port> — launches a fresh server listening on <port> with
# snapshotting disabled, and waits for it to actually service commands.
start_server() {
  local port="$1"
  if [[ -z "${SERVER_BIN:-}" || ! -x "$SERVER_BIN" ]]; then
    echo "error: my_redis_server binary not found; build v2 first " \
         "(cmake -B build/v2/debug -S v2 -DCMAKE_BUILD_TYPE=Debug && " \
         "cmake --build build/v2/debug) or set SERVER_BIN explicitly" >&2
    exit 1
  fi
  "$SERVER_BIN" -p "$port" -s 0 >"$SERVER_LOG" 2>&1 &
  SERVER_PID=$!

  local tries=100
  while (( tries-- > 0 )); do
    port_open "$port" && break
    kill -0 "$SERVER_PID" 2>/dev/null || {
      echo "error: server exited before it started listening; log:" >&2
      cat "$SERVER_LOG" >&2
      exit 1
    }
    sleep 0.05
  done
  (( tries > -1 )) || {
    echo "error: server never started listening on $port" >&2
    exit 1
  }

  # listen() runs in the server's constructor, well before Run() starts its
  # epoll_wait loop (io-thread setup and a startup log line happen in
  # between). port_open() above only proves the kernel completed a TCP
  # handshake off the listen backlog, not that anything is reading yet —
  # a command sent right after could sit unread and time out in
  # send_command. Wait for a real round trip before handing off to the test.
  tries=20
  while (( tries-- > 0 )); do
    [[ "$(send_command "$port" PING)" == "$(printf '+PONG\r\n')" ]] && return 0
  done
  echo "error: server never became responsive on $port" >&2
  exit 1
}

stop_server() {
  [[ -n "$SERVER_PID" ]] || return 0
  kill "$SERVER_PID" 2>/dev/null
  wait "$SERVER_PID" 2>/dev/null
  SERVER_PID=""
  rm -f "$SERVER_LOG"
}

trap stop_server EXIT INT TERM

# resp_encode arg... — writes a RESP array-of-bulk-strings request (the wire
# form every client command takes) to stdout.
resp_encode() {
  printf '*%d\r\n' "$#"
  local arg
  for arg in "$@"; do
    printf '$%d\r\n%s\r\n' "${#arg}" "$arg"
  done
}

# send_command <port> <arg...> — opens a fresh connection, sends one RESP
# command, and prints the raw reply bytes read back within the timeout.
send_command() {
  local port="$1"; shift
  local reply
  exec 9<>"/dev/tcp/$HOST/$port"
  resp_encode "$@" >&9
  reply="$(timeout 1 cat <&9)"
  exec 9>&- 9<&-
  printf '%s' "$reply"
}

# expect_eq <description> <actual> <expected>
expect_eq() {
  local desc="$1" actual="$2" expected="$3"
  if [[ "$actual" == "$expected" ]]; then
    echo "  ok   - $desc"
    PASS=$((PASS + 1))
  else
    echo "  FAIL - $desc"
    printf '         expected: %q\n' "$expected"
    printf '         actual:   %q\n' "$actual"
    FAIL=$((FAIL + 1))
  fi
}

# summary — prints the pass/fail tally and returns non-zero if anything failed.
summary() {
  echo "  -- $PASS passed, $FAIL failed --"
  (( FAIL == 0 ))
}
