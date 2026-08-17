#!/usr/bin/env bash
# e2e test for GetRequestHandler (server/handler/get_request_handler.h).
set -uo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"

PORT=6394
start_server "$PORT"

expect_eq "GET on a missing key replies with a null bulk string" \
  "$(send_command "$PORT" GET missing-key)" \
  "$(printf '$-1\r\n')"

send_command "$PORT" SET foo bar >/dev/null

expect_eq "GET returns a previously SET value" \
  "$(send_command "$PORT" GET foo)" \
  "$(printf '$3\r\nbar\r\n')"

expect_eq "GET with an empty key is unrecognised" \
  "$(send_command "$PORT" GET "")" \
  "$(printf -- '-Unknown subcommand or command\r\n')"

summary
