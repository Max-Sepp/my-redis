#!/usr/bin/env bash
# e2e test for PingRequestHandler (server/handler/ping_request_handler.h).
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6390
start_server "$PORT"

expect_eq "PING with no args replies PONG" \
  "$(send_command "$PORT" PING)" \
  "$(printf '+PONG\r\n')"

expect_eq "PING with a message returns it as a bulk-string reply" \
  "$(send_command "$PORT" PING "hello world")" \
  "$(printf '$11\r\nhello world\r\n')"

expect_eq "PING with too many args is unrecognised" \
  "$(send_command "$PORT" PING one two)" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
